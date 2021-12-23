/******************************
 * Wayland クライアント処理
 ******************************/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define __USE_GNU
#include <fcntl.h>
#include <unistd.h>
#include <poll.h>

#include <wayland-client.h>

#include "client.h"
#include "imagebuf.h"


//========================
// 破棄
//========================


/* wl_pointer 破棄 */

static void _pointer_release(Client *p)
{
	if(p->seat_ver >= WL_POINTER_RELEASE_SINCE_VERSION)
		wl_pointer_release(p->pointer);
	else
		wl_pointer_destroy(p->pointer);

	p->pointer = NULL;
}

/* wl_keyboard 破棄 */

static void _keyboard_release(Client *p)
{
	if(p->seat_ver >= WL_KEYBOARD_RELEASE_SINCE_VERSION)
		wl_keyboard_release(p->keyboard);
	else
		wl_keyboard_destroy(p->keyboard);

	p->keyboard = NULL;
}


//========================
// display 同期
//========================


static void _display_callback_done(void *data,struct wl_callback *callback,uint32_t time)
{
	wl_callback_destroy(callback);

	((Client *)data)->disp_sync_cnt--;
}

static const struct wl_callback_listener g_display_sync_listener = {
	_display_callback_done
};

//========================
// xdg_wm_base
//========================
static void xdg_wm_base_ping_handler(void *data,
        struct xdg_wm_base *xdg_wm_base, uint32_t serial)
{
    xdg_wm_base_pong(xdg_wm_base, serial);
}

static const struct xdg_wm_base_listener xdg_wm_base_listener = {
    .ping = xdg_wm_base_ping_handler,
};

//========================
// wl_shell_surface
//========================


static void _shell_surface_ping(
	void *data,struct wl_shell_surface *shell_surface,uint32_t serial)
{
	wl_shell_surface_pong(shell_surface, serial);
}

static void _shell_surface_configure(
	void *data,struct wl_shell_surface *shell_surface,
	uint32_t edges,int32_t width,int32_t height)
{
	Window *p = (Window *)data;

	if(p->configure)
		(p->configure)(p, width, height);
}

static void _shell_surface_popup_done(void *data,struct wl_shell_surface *shell_surface)
{

}

static void _xdg_surface_configure(void *data, struct xdg_surface *xdg_surface, uint32_t serial)
{
    xdg_surface_ack_configure(xdg_surface, serial);
}

static const struct xdg_surface_listener g_xdg_surface_listener = {
    .configure = _xdg_surface_configure,
};


//========================
// wl_seat
//========================


static void _seat_capabilities(void *data,struct wl_seat *seat,uint32_t cap)
{
	Client *p = (Client *)data;

	//wl_pointer

	if(p->init_flags & INIT_FLAGS_POINTER)
	{
		if((cap & WL_SEAT_CAPABILITY_POINTER) && !p->pointer)
		{
			p->pointer = wl_seat_get_pointer(seat);

			wl_pointer_add_listener(p->pointer, p->pointer_listener, p);
		}
		else if(!(cap & WL_SEAT_CAPABILITY_POINTER) && p->pointer)
			_pointer_release(p);
	}

	//wl_keyboard

	if(p->init_flags & INIT_FLAGS_KEYBOARD)
	{
		if((cap & WL_SEAT_CAPABILITY_KEYBOARD) && !p->keyboard)
		{
			p->keyboard = wl_seat_get_keyboard(seat);

			wl_keyboard_add_listener(p->keyboard, p->keyboard_listener, p);
		}
		else if(!(cap & WL_SEAT_CAPABILITY_KEYBOARD) && p->keyboard)
			_keyboard_release(p);
	}
	
	//ほか

	if(p->seat_capabilities)
		(p->seat_capabilities)(p, seat, cap);
}

static void _seat_name(void *data,struct wl_seat *wl_seat,const char *name)
{

}

static const struct wl_seat_listener g_seat_listener = {
	_seat_capabilities, _seat_name
};


//========================
// wl_registry
//========================


static void _registry_global(
	void *data,struct wl_registry *reg,uint32_t id,const char *name,uint32_t ver)
{
    fprintf(stderr, "_registry_global\n");
	Client *p = (Client *)data;

    if(strcmp(name, "wl_compositor") == 0) {
		p->compositor = wl_registry_bind(reg, id, &wl_compositor_interface, 1);
    } else if (strcmp(name, "wl_shm") == 0) {
		p->shm = wl_registry_bind(reg, id, &wl_shm_interface, 1);

    } else if(strcmp(name, "xdg_wm_base") == 0) {
        p->wm_base = wl_registry_bind(reg, id, &xdg_wm_base_interface, 1);
        fprintf(stderr, "wm_base registered!\n");
        xdg_wm_base_add_listener(p->wm_base, &xdg_wm_base_listener, NULL);
    } else if((p->init_flags & INIT_FLAGS_SEAT)
        && strcmp(name, "wl_seat") == 0) {
		/* Client::registry_global で wl_seat を処理する場合もあるので
		 * フラグが ON の時のみ処理 */

		if(ver >= 5) ver = 5;
		
		p->seat = wl_registry_bind(reg, id, &wl_seat_interface, ver);
		p->seat_ver = ver;

		wl_seat_add_listener(p->seat, &g_seat_listener, p);

		//引き続き、ハンドラ内で wl_pointer などを作成したいので、同期させる

		Client_add_init_sync(p);
    } else if(p->registry_global) {
		(p->registry_global)(data, reg, id, name, ver);
    }
}

static void _registry_global_remove(void *data,struct wl_registry *registry,uint32_t id)
{
}

static const struct wl_registry_listener g_registry_listener = {
    .global = _registry_global,
    .global_remove = _registry_global_remove,
};


//========================
// Client
//========================


/* クライアント終了 */

void Client_destroy(Client *p)
{
	if(p)
	{
		//破棄ハンドラ
	
		if(p->destroy)
			(p->destroy)(p);
	
		//poll

		Client_poll_clear(p);
	
		//wl_seat

		if(p->pointer)
			_pointer_release(p);
		
		if(p->seat)
		{
			if(p->seat_ver >= WL_SEAT_RELEASE_SINCE_VERSION)
				wl_seat_release(p->seat);
			else
				wl_seat_destroy(p->seat);
		}

		//
	
		wl_shm_destroy(p->shm);
//		wl_shell_destroy(p->shell);
        xdg_wm_base_destroy(p->wm_base);
		wl_compositor_destroy(p->compositor);

		wl_registry_destroy(p->registry);
	
		wl_display_disconnect(p->display);

		free(p);
	}
}

/* Client データ確保
 *
 * size: 構造体のサイズ。sizeof(Client) 以下なら自動で最低サイズ。 */

Client *Client_new(int size)
{
	Client *p;

	if(size < sizeof(Client))
		size = sizeof(Client);

	p = (Client *)calloc(1, size);
	if(!p) return NULL;

	wl_list_init(&p->list_poll);

	return p;
}

/* Wayland クライアントの初期化 */

void Client_init(Client *p)
{
	struct wl_display *disp;

	//接続

	disp = p->display = wl_display_connect(NULL);

	if(!disp)
	{
		printf("failed wl_display_connect\n");
		exit(1);
	}

	//wl_registry

	p->registry = wl_display_get_registry(disp);

	wl_registry_add_listener(p->registry, &g_registry_listener, p);

	Client_add_init_sync(p);

	//初期処理が終わるまで待つ

	while(wl_display_dispatch(disp) != -1 && p->disp_sync_cnt);
}

/* 初期化時の同期要求を追加
 *
 * 待つ必要があれば、バインドしてハンドラを設定した回数分実行する */

void Client_add_init_sync(Client *p)
{
	struct wl_callback *callback;

	callback = wl_display_sync(p->display);

	wl_callback_add_listener(callback, &g_display_sync_listener, p);

	p->disp_sync_cnt++;
}

/* イベントループ (単純) */

void Client_loop_simple(Client *p)
{
	while(wl_display_dispatch(p->display) != -1 && p->finish_loop == 0);
}

/* イベントループ (poll) */

void Client_loop_poll(Client *p)
{
	struct pollfd fds[10];
	int i,num;
	PollItem *pi,*ptr[10];

	//fds[0] は Wayland イベント用

	fds[0].fd = wl_display_get_fd(p->display);
	fds[0].events = POLLIN;

	//

	while(!p->finish_loop)
	{
		wl_display_flush(p->display);

		//fds にセット (1〜)

		num = 1;

		wl_list_for_each(pi, &p->list_poll, i)
		{
			fds[num].fd = pi->fd;
			fds[num].events = pi->events;
			ptr[num] = pi;

			num++;
			if(num == 10) break;
		}

		//

		poll(fds, num, -1);

		if(fds[0].revents & POLLIN)
			wl_display_dispatch(p->display);

		for(i = 1; i < num; i++)
		{
			if(fds[i].revents)
				(ptr[i]->handle)(p, fds[i].fd, fds[i].revents);
		}
	}
}

/* poll 追加 */

void Client_poll_add(Client *p,int fd,int events,poll_handle handle)
{
	PollItem *pi;

	pi = (PollItem *)calloc(1, sizeof(PollItem));
	if(!pi) return;

	pi->fd = fd;
	pi->events = events;
	pi->handle = handle;

	wl_list_insert(p->list_poll.prev, &pi->i);
}

/* poll 削除 */

void Client_poll_delete(Client *p,int fd)
{
	PollItem *pi;

	wl_list_for_each(pi, &p->list_poll, i)
	{
		if(pi->fd == fd)
		{
			close(fd);
			wl_list_remove(&pi->i);

			free(pi);
			break;
		}
	}
}

/* poll 全て削除 */

void Client_poll_clear(Client *p)
{
	PollItem *pi,*tmp;

	wl_list_for_each_safe(pi, tmp, &p->list_poll, i)
	{
		close(pi->fd);
		
		wl_list_remove(&pi->i);

		free(pi);
	}
}


//========================
// Window
//========================


/* ウィンドウ作成
 *
 * listener: NULL でデフォルト。ハンドラを独自設定する場合に指定。 */

Window *Window_create(Client *cl,int width,int height,
    const struct xdg_surface_listener *listener)
{
	Window *p;

	p = (Window *)calloc(1, sizeof(Window));
	if(!p) {
        fprintf(stderr, "!p. return NULL\n");
        return NULL;
    }
    fprintf(stderr, "p allocated!\n");

	p->client = cl;
	p->width = width;
	p->height = height;

	//wl_surface

	p->surface = wl_compositor_create_surface(cl->compositor);

	//wl_shell_surface
	
//	p->shell_surface = wl_shell_get_shell_surface(cl->shell, p->surface);
    p->xdg_surface = xdg_wm_base_get_xdg_surface(cl->wm_base, p->surface);

//    xdg_surface_set_toplevel(p->xdg_surface);
    p->toplevel = xdg_surface_get_toplevel(p->xdg_surface);

    xdg_surface_add_listener(p->xdg_surface,
        (listener)? listener: &g_xdg_surface_listener,
		p);

	//イメージ作成

	p->img = ImageBuf_create(cl->shm, width, height);

	return p;
}

/* ウィンドウ破棄 */

void Window_destroy(Window *p)
{
	if(p)
	{
        xdg_surface_destroy(p->xdg_surface);
		wl_surface_destroy(p->surface);

		ImageBuf_destroy(p->img);

		free(p);
	}
}

/* ウィンドウ更新
 *
 * 常にアルファ処理を行う */

void Window_update(Window *p)
{
	wl_surface_attach(p->surface, p->img->buffer, 0, 0);
	wl_surface_damage(p->surface, 0, 0, p->width, p->height);

	wl_surface_commit(p->surface);
}

/* ウィンドウ更新
 *
 * すべて不透明として扱う */

void Window_updateOpaque(Window *p)
{
	struct wl_region *region;

	wl_surface_attach(p->surface, p->img->buffer, 0, 0);
	wl_surface_damage(p->surface, 0, 0, p->width, p->height);

	//完全不透明範囲セット
	region = wl_compositor_create_region(p->client->compositor);
	wl_region_add(region, 0, 0, p->width, p->height);
	wl_surface_set_opaque_region(p->surface, region);
	wl_region_destroy(region);

	wl_surface_commit(p->surface);
}
