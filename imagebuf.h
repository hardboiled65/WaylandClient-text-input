#ifndef _IMAGEBUF_H_
#define _IMAGEBUF_H_

/* 共有メモリイメージ */

typedef struct _ImageBuf ImageBuf;

struct _ImageBuf
{
	struct wl_shm_pool *pool;
	struct wl_buffer *buffer;
	void *data;
	int width,
		height,
		size;
};

ImageBuf *ImageBuf_create(struct wl_shm *shm,int width,int height);
void ImageBuf_destroy(ImageBuf *p);

void ImageBuf_setPixel(ImageBuf *p,int x,int y,uint32_t col);
void ImageBuf_fill(ImageBuf *p,uint32_t col);
void ImageBuf_fillH(ImageBuf *p,int y,int h,uint32_t col);
void ImageBuf_box(ImageBuf *p,int x,int y,int w,int h,uint32_t col);

#endif
