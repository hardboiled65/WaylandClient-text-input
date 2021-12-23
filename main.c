/******************************
 * text-input-unstable-v3
 ******************************/

#include <stdio.h>
#include <string.h>
#include <linux/input.h>

#include <wayland-client.h>
#include "text-input-unstable-v3-client-protocol.h"

#include "client.h"
#include "imagebuf.h"


//-------------

struct zwp_text_input_manager_v3 *g_input_manager = NULL;
struct zwp_text_input_v3 *g_text_input;

#define INPUTBOX_X 10
#define INPUTBOX_Y 10
#define INPUTBOX_W 200
#define INPUTBOX_H 60

//-------------


//-----------------------
// zwp_text_input_v3
//-----------------------


/* enter */

static void _input_enter(void *data, struct zwp_text_input_v3 *text_input,
	struct wl_surface *surface)
{
	printf("text_input # enter\n");

	zwp_text_input_v3_enable(text_input);

	zwp_text_input_v3_set_cursor_rectangle(text_input,
		INPUTBOX_X, INPUTBOX_Y, INPUTBOX_W, INPUTBOX_H);

	zwp_text_input_v3_commit(text_input);
}

/* leave */

static void _input_leave(void *data, struct zwp_text_input_v3 *text_input,
	struct wl_surface *surface)
{
	printf("text_input # leave\n");

	zwp_text_input_v3_disable(text_input);
	zwp_text_input_v3_commit(text_input);
}

/* preedit_string */

static void _input_preedit_string(void *data, struct zwp_text_input_v3 *text_input,
	const char *text, int32_t cursor_begin, int32_t cursor_end)
{
	printf("text_input # preedit_string | text:\"%s\", cursor_begin:%d, cursor_end:%d\n",
		text, cursor_begin, cursor_end);
}

/* commit_string */

static void _input_commit_string(void *data, struct zwp_text_input_v3 *text_input,
	const char *text)
{
	printf("text_input # commit_string | text:\"%s\"\n", text);
}

/* delete_surrounding_text */

static void _input_delete_surrounding_text(void *data,
	struct zwp_text_input_v3 *text_input,
	uint32_t before_length, uint32_t after_length)
{
	printf("text_input # delete_surrounding_text | before_length:%u, after_length:%u\n",
		before_length, after_length);
}

/* done */

static void _input_done(void *data, struct zwp_text_input_v3 *text_input,
	uint32_t serial)
{
	printf("text_input # done | serial:%u\n", serial);

#if 0
	//周囲のテキストセット
	
	zwp_text_input_v3_set_surrounding_text(text_input,
		"[text]", 1, 5);

	zwp_text_input_v3_set_text_change_cause(text_input,
		ZWP_TEXT_INPUT_V3_CHANGE_CAUSE_INPUT_METHOD);

	zwp_text_input_v3_commit(text_input);
#endif
}


static const struct zwp_text_input_v3_listener g_text_input_listener = {
	_input_enter, _input_leave, _input_preedit_string,
	_input_commit_string, _input_delete_surrounding_text, _input_done
};


//-----------------------
// wl_keyboard
//-----------------------


static void _keyboard_keymap(void *data, struct wl_keyboard *keyboard,
	uint32_t format, int32_t fd, uint32_t size)
{
}

static void _keyboard_enter(void *data, struct wl_keyboard *keyboard,
	uint32_t serial, struct wl_surface *surface, struct wl_array *keys)
{
	printf("wl_keyboard # enter\n");
}

static void _keyboard_leave(void *data, struct wl_keyboard *keyboard,
	uint32_t serial, struct wl_surface *surface)
{
	printf("wl_keyboard # leave\n");
}

/* キー */

static void _keyboard_key(void *data, struct wl_keyboard *keyboard,
	uint32_t serial, uint32_t time, uint32_t key, uint32_t state)
{
	Client *p = (Client *)data;

	printf("wl_keyboard # key | %s, key:%u\n",
		(state == WL_KEYBOARD_KEY_STATE_PRESSED)? "press":"release", key);

	if(state != WL_KEYBOARD_KEY_STATE_PRESSED) return;

	switch(key)
	{
		//ESC キーで終了
		case KEY_ESC:
			p->finish_loop = 1;
			break;
	}
}

static void _keyboard_modifiers(void *data, struct wl_keyboard *keyboard,
	uint32_t serial, uint32_t mods_depressed, uint32_t mods_latched, uint32_t mods_locked, uint32_t group)
{

}

static void _keyboard_repeat_info(void *data, struct wl_keyboard *keyboard,
	int32_t rate, int32_t delay)
{

}

static const struct wl_keyboard_listener g_keyboard_listener = {
	_keyboard_keymap, _keyboard_enter, _keyboard_leave, _keyboard_key,
	_keyboard_modifiers, _keyboard_repeat_info
};


//------------------------


static void _registry_global(
    void *data,struct wl_registry *reg,uint32_t id,const char *name,uint32_t ver)
{
	if(strcmp(name, "zwp_text_input_manager_v3") == 0)
	{
		g_input_manager = wl_registry_bind(reg, id,
			&zwp_text_input_manager_v3_interface, 1);
	}
}


//---------------


int main(void)
{
	Client *p;
	Window *win;

	p = Client_new(0);

	p->init_flags = INIT_FLAGS_SEAT | INIT_FLAGS_KEYBOARD;
	p->keyboard_listener = &g_keyboard_listener;
	p->registry_global = _registry_global;
	
	Client_init(p);

	if(!g_input_manager)
	{
		printf("[!] not found 'zwp_text_input_manager_v3'\n");
		Client_destroy(p);
		return 1;
	}

	//zwp_text_input

	g_text_input = zwp_text_input_manager_v3_get_text_input(
		g_input_manager, p->seat);

	zwp_text_input_v3_add_listener(g_text_input,
		&g_text_input_listener, p);

	//ウィンドウ

	win = Window_create(p, 256, 256, NULL);

	ImageBuf_fill(win->img, 0xffff0000);

	ImageBuf_box(win->img,
		INPUTBOX_X, INPUTBOX_Y, INPUTBOX_W, INPUTBOX_H,
		0xff000000);
	
	Window_update(win);

	//

    Client_loop_simple(p);

	//解放

	Window_destroy(win);

	zwp_text_input_v3_destroy(g_text_input);
	zwp_text_input_manager_v3_destroy(g_input_manager);

	Client_destroy(p);

	return 0;
}
