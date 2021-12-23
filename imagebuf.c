/******************************
 * 共有メモリイメージ
 ******************************/

#include <stdlib.h>
#include <stdio.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include <wayland-client.h>

#include "client.h"
#include "imagebuf.h"


//共有メモリ名で使う
static uint32_t g_shm_cnt = 0;


/* POSIX 共有メモリオブジェクトを作成 */

static int _create_posix_shm(void)
{
	char name[64];
	int ret;

	while(1)
	{
		snprintf(name, 64, "/wayland-test-%x", g_shm_cnt);
		
		ret = shm_open(name, O_CREAT | O_EXCL | O_RDWR | O_CLOEXEC, 0600);

		if(ret >= 0)
		{
			//成功
			shm_unlink(name);
			g_shm_cnt++;

			break;
		}
		else if(errno == EEXIST)
			//同名が存在する
			g_shm_cnt++;
		else if(errno != EINTR)
			break;
	}

	return ret;
}

/* wl_shm_pool 作成 */

static struct wl_shm_pool *_create_shm_pool(struct wl_shm *shm,int size,void **ppbuf)
{
	int fd;
	void *data;
	struct wl_shm_pool *pool;

	fd = _create_posix_shm();
	if(fd < 0) return NULL;

	//サイズ変更

	if(ftruncate(fd, size) < 0)
	{
		close(fd);
		return NULL;
	}

	//メモリにマッピング

	data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

	if(data == MAP_FAILED)
	{
		close(fd);
		return NULL;
	}

	//wl_shm_pool 作成

	pool = wl_shm_create_pool(shm, fd, size);

	close(fd);

	*ppbuf = data;

	return pool;
}


//=====================


/* 作成 */

ImageBuf *ImageBuf_create(struct wl_shm *shm,int width,int height)
{
	ImageBuf *img;
	struct wl_shm_pool *pool;
	struct wl_buffer *buffer;
	void *data;
	int size;

	size = width * 4 * height;

	//wl_shm_pool 作成

	pool = _create_shm_pool(shm, size, &data);
	if(!pool) return NULL;

	//wl_buffer 作成

	buffer = wl_shm_pool_create_buffer(pool,
		0, width, height,
		width * 4,
		WL_SHM_FORMAT_ARGB8888);

	if(!buffer) goto ERR;

	//ImageBuf 作成

	img = (ImageBuf *)malloc(sizeof(ImageBuf));
	if(!img) goto ERR;

	img->pool = pool;
	img->buffer = buffer;
	img->data = data;
	img->width = width;
	img->height = height;
	img->size = size;

	return img;

ERR:
	wl_shm_pool_destroy(pool);
	munmap(data, size);
	return NULL;
}

/* 削除 */

void ImageBuf_destroy(ImageBuf *p)
{
	if(p)
	{
		wl_buffer_destroy(p->buffer);
		wl_shm_pool_destroy(p->pool);
		munmap(p->data, p->size);
		
		free(p);
	}
}

/* 点を打つ */

void ImageBuf_setPixel(ImageBuf *p,int x,int y,uint32_t col)
{
	if(x >= 0 && y >= 0 && x < p->width && y < p->height)
		*((uint32_t *)p->data + y * p->width + x) = col;
}

/* 塗りつぶし */

void ImageBuf_fill(ImageBuf *p,uint32_t col)
{
	uint32_t *pd = (uint32_t *)p->data;
	int i;

	for(i = p->width * p->height; i > 0; i--)
		*(pd++) = col;
}

/* 指定位置から指定高さ分塗りつぶし */

void ImageBuf_fillH(ImageBuf *p,int y,int h,uint32_t col)
{
	uint32_t *pd;
	int i;

	pd = (uint32_t *)p->data + y * p->width;

	for(i = p->width * h; i > 0; i--)
		*(pd++) = col;
}

/* 四角形枠 */

void ImageBuf_box(ImageBuf *p,int x,int y,int w,int h,uint32_t col)
{
	int i,n;

	n = y + h - 1;

	for(i = 0; i < w; i++)
	{
		ImageBuf_setPixel(p, x + i, y, col);
		ImageBuf_setPixel(p, x + i, n, col);
	}

	n = x + w - 1;

	for(i = 1; i < h - 1; i++)
	{
		ImageBuf_setPixel(p, x, y + i, col);
		ImageBuf_setPixel(p, n, y + i, col);
	}
}
