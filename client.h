/*============================
 * Wayland クライアント
 *============================*/

#ifndef _CLIENT_H_
#define _CLIENT_H_

#include <wayland-client.h>
#include "xdg-shell-client-protocol.h"

typedef struct _Client Client;
typedef struct _ImageBuf ImageBuf;


/*---- poll ----*/

typedef void (*poll_handle)(Client *p,int fd,int events);

typedef struct
{
	struct wl_list i;
	int fd,
		events;
	poll_handle handle;
}PollItem;


/*---- Client ----*/

#define CLIENT(p)  ((Client *)(p))

typedef void (*client_registry_global)(
	void *data,struct wl_registry *reg,uint32_t id,const char *name,uint32_t ver);

typedef void (*client_seat_capabilities)(Client *p,struct wl_seat *seat,uint32_t cap);


struct _Client
{
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shm *shm;
//	struct wl_shell *shell;
    struct xdg_wm_base *wm_base;
	struct wl_seat *seat;
	struct wl_pointer *pointer;
	struct wl_keyboard *keyboard;

	struct wl_list list_poll;	//poll のリスト

	uint32_t init_flags,	//初期化時に処理するフラグ (初期化前に明示的にセット)
		seat_ver,			//wl_seat のバージョン
		disp_sync_cnt;		//同期を待つ回数

	int finish_loop;	//0 以外にすると、イベントループを抜ける

	//破棄時のハンドラ
	void (*destroy)(Client *p);
	//wl_registry:global イベント (必須インターフェイス以外の処理時)
	client_registry_global registry_global;
	//wl_seat:capabilities イベント
	client_seat_capabilities seat_capabilities;

	const struct wl_pointer_listener *pointer_listener;		//wl_pointer のハンドラ構造体
	const struct wl_keyboard_listener *keyboard_listener;	//wl_keyboard のハンドラ構造体
};

enum
{
	INIT_FLAGS_SEAT = 1<<0,
	INIT_FLAGS_POINTER = 1<<1,
	INIT_FLAGS_KEYBOARD = 1<<2
};

Client *Client_new(int size);
void Client_destroy(Client *p);
void Client_loop_simple(Client *p);

void Client_init(Client *p);
void Client_add_init_sync(Client *p);

/*---- Window ----*/

typedef struct _Window Window;

typedef void (*window_configure)(Window *p,int width,int height);

struct _Window
{
	Client *client;
	struct wl_surface *surface;
//	struct wl_shell_surface *shell_surface;
    struct xdg_toplevel *toplevel;
    struct xdg_surface *xdg_surface;
	ImageBuf *img;
	int width,height;

	window_configure configure;
};

Window *Window_create(Client *cl,int width,int height,
    const struct xdg_surface_listener *listener);

void Window_destroy(Window *p);
void Window_update(Window *p);
void Window_updateOpaque(Window *p);

#endif
