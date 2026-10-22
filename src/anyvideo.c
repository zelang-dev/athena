#include <athena.h>

/* Overlay buttons layout  `x, y, w, h` and label. */
typedef struct {
	int x, y, w, h;
	const char *label;
} overlay_rect;

#define HIDE_TIMEOUT_MS 2000
#define OVERLAY_HEIGHT 40
#define PROGRESS_HEIGHT 50

#include "../share/pixmaps/movie.xpm"

static void pip_resize(ats_video_info *ctx, int w, int h) {
	pthread_mutex_lock(&ctx->media_mutex);
	if (ctx->pip_w == w && ctx->pip_h == h && ctx->pip_ximage != NULL) {
		pthread_mutex_unlock(&ctx->media_mutex);
		return;
	}

	ctx->pip_w = w;
	ctx->pip_h = h;
	if (ctx->pip_ximage) {
		XDestroyImage(ctx->pip_ximage);
		ctx->pip_ximage = NULL;
	}

	if (ctx->pip_sws_ctx) {
		sws_freeContext(ctx->pip_sws_ctx);
		ctx->pip_sws_ctx = NULL;
	}
	pthread_mutex_unlock(&ctx->media_mutex);
}

static void toggle_pip(ats_video_info *ctx) {
	ctx->pip_mode = !ctx->pip_mode;
	XLockDisplay(ctx->display);
	if (ctx->pip_ximage) {
		XDestroyImage(ctx->pip_ximage);
		ctx->pip_ximage = NULL;
	}

	if (ctx->pip_sws_ctx) {
		sws_freeContext(ctx->pip_sws_ctx);
		ctx->pip_sws_ctx = NULL;
	}

	if (ctx->pip_mode) {
		int scr = DefaultScreen(ctx->display);
		ctx->pip_win = XCreateSimpleWindow(ctx->display, RootWindow(ctx->display, scr), 0, 0,
			ctx->pip_w, ctx->pip_h, 0, WhitePixel(ctx->display, scr), BlackPixel(ctx->display, scr));
		ats_display_wm(ctx->display, ctx->pip_win, movie, NULL, NULL);
		XStoreName(ctx->display, ctx->pip_win, "PiP View");
		XSelectInput(ctx->display, ctx->pip_win, StructureNotifyMask);
		XMapWindow(ctx->display, ctx->pip_win);
		ctx->pip_gc = XCreateGC(ctx->display, ctx->pip_win, 0, NULL);
	} else if (ctx->pip_win) {
		XFreeGC(ctx->display, ctx->pip_gc);
		XDestroyWindow(ctx->display, ctx->pip_win);
		ctx->pip_win = 0;
		ctx->pip_gc = NULL;
	}
	XClearWindow(ctx->display, ctx->win);
	XUnlockDisplay(ctx->display);
}

static void toggle_fullscreen(ats_video_info *ctx) {
	XLockDisplay(ctx->display);
	ctx->is_fullscreen = !ctx->is_fullscreen;
	if (ctx->is_fullscreen) {
		XWindowAttributes wa;
		XGetWindowAttributes(ctx->display, ctx->top_win, &wa);
		ctx->saved_x = wa.x; ctx->saved_y = wa.y;
		ctx->saved_w = wa.width; ctx->saved_h = wa.height;
		int scr = DefaultScreen(ctx->display);
		Screen *screen = ScreenOfDisplay(ctx->display, scr);
		int scr_w = screen->width;
		int scr_h = screen->height;
		/* Apply border override flags to cover window decorations completely */
		XSetWindowAttributes swa;
		swa.override_redirect = True;
		XChangeWindowAttributes(ctx->display, ctx->top_win, CWOverrideRedirect, &swa);
		XMoveResizeWindow(ctx->display, ctx->top_win, 0, 0, scr_w, scr_h);
		XRaiseWindow(ctx->display, ctx->top_win);
		ctx->win_w = scr_w; ctx->win_h = scr_h;
		ctx->full_aspect = (double)ctx->win_w / ctx->win_h;
	} else {
		XSetWindowAttributes swa;
		swa.override_redirect = False;
		XChangeWindowAttributes(ctx->display, ctx->top_win, CWOverrideRedirect, &swa);
		XMoveResizeWindow(ctx->display, ctx->top_win, ctx->saved_x, ctx->saved_y, ctx->saved_w, ctx->saved_h);
		ctx->win_w = ctx->saved_w; ctx->win_h = ctx->saved_h;
	}
	XClearWindow(ctx->display, ctx->top_win);
	XUnlockDisplay(ctx->display);
}

// Cleanup after thread loop exits
static void free_video(ats_video_info *ctx) {
	if (ctx->gc)
		XFreeGC(ctx->display, ctx->gc);

	ctx->gc = NULL;
	if (ctx->pip_gc)
		XFreeGC(ctx->display, ctx->pip_gc);

	ctx->pip_gc = NULL;
	if (ctx->pcm_handle) {
		snd_pcm_drain(ctx->pcm_handle);
		snd_pcm_close(ctx->pcm_handle);
	}

	ctx->pcm_handle = NULL;
	if (ctx->sws_ctx)
		sws_freeContext(ctx->sws_ctx);

	ctx->sws_ctx = NULL;
	if (ctx->pip_sws_ctx)
		sws_freeContext(ctx->pip_sws_ctx);

	ctx->pip_sws_ctx = NULL;
	if (ctx->ximage)
		XDestroyImage(ctx->ximage);

	ctx->ximage = NULL;
	if (ctx->pip_ximage)
		XDestroyImage(ctx->pip_ximage);

	ctx->pip_ximage = NULL;
	if (ctx->swr_ctx)
		swr_free(&ctx->swr_ctx);

	ctx->swr_ctx = NULL;
	if (ctx->video_ctx)
		avcodec_free_context(&ctx->video_ctx);

	ctx->video_ctx = NULL;
	if (ctx->audio_ctx)
		avcodec_free_context(&ctx->audio_ctx);

	ctx->audio_ctx = NULL;
	if (ctx->fmt_ctx)
		avformat_close_input(&ctx->fmt_ctx);

	ctx->fmt_ctx = NULL;
}

// Configures Native ALSA device for S16 Interleaved playback matching audio stream metadata
static int init_alsa(ats_video_info *ctx, unsigned int rate, int channels) {
	int rc = snd_pcm_open(&ctx->pcm_handle, "default", SND_PCM_STREAM_PLAYBACK, 0);
	if (rc < 0) return -1;

	snd_pcm_hw_params_t *params;
	snd_pcm_hw_params_alloca(&params);
	snd_pcm_hw_params_any(ctx->pcm_handle, params);

	snd_pcm_hw_params_set_access(ctx->pcm_handle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	snd_pcm_hw_params_set_format(ctx->pcm_handle, params, SND_PCM_FORMAT_S16_LE);
	snd_pcm_hw_params_set_channels(ctx->pcm_handle, params, channels);
	snd_pcm_hw_params_set_rate_near(ctx->pcm_handle, params, &rate, 0);

	rc = snd_pcm_hw_params(ctx->pcm_handle, params);
	if (rc < 0) {
		snd_pcm_close(ctx->pcm_handle);
		ctx->pcm_handle = NULL;
		return -1;
	}

	ctx->has_audio = true;
	return 0;
}

static FORCEINLINE int media_click_check(overlay_rect b, int mx, int my) {
	return (mx >= b.x && mx <= b.x + b.w && my >= b.y && my <= b.y + b.h);
}

void media_overlay_controls(ats_video_info *ctx, int win_w, int win_h) {
	if (!ctx->gc)
		return;

	XLockDisplay(ctx->display);
	// Control Bar Background
	XSetForeground(ctx->display, ctx->gc, 0x1A1A1A);
	XFillRectangle(ctx->display, ctx->win, ctx->gc, 0, win_h - 50, win_w, 50);

	// Play / Pause Indicator
	XSetForeground(ctx->display, ctx->gc, 0xaaaaaa);
	if (ctx->state == STATE_PLAYING) {
		// Pause icon lines
		XFillRectangle(ctx->display, ctx->win, ctx->gc, 20, win_h - 35, 5, 20);
		XFillRectangle(ctx->display, ctx->win, ctx->gc, 32, win_h - 35, 5, 20);
	} else {
		// Play icon triangle
		XPoint points[3] = {{20, win_h - 35}, {20, win_h - 15}, {35, win_h - 25}};
		XFillPolygon(ctx->display, ctx->win, ctx->gc, points, 3, Convex, CoordModeOrigin);
	}

	// Volume Level Indicator
	XDrawArc(ctx->display, ctx->win, ctx->gc, 54, win_h - 31, 6, 12, -45 * 64, 90 * 64);
	XDrawArc(ctx->display, ctx->win, ctx->gc, 50, win_h - 30, 6, 10, -45 * 64, 90 * 64);
	XDrawArc(ctx->display, ctx->win, ctx->gc, 46, win_h - 29, 6, 8, -45 * 64, 90 * 64);
	XFillRectangle(ctx->display, ctx->win, ctx->gc, 46, win_h - 28, 2, 6);

	XSetForeground(ctx->display, ctx->gc, 0x888888);
	XFillRectangle(ctx->display, ctx->win, ctx->gc, 64, win_h - 28, 80, 6);
	XSetForeground(ctx->display, ctx->gc, 0x555555);
	XFillRectangle(ctx->display, ctx->win, ctx->gc, 64, win_h - 28, (int)(80 * ctx->volume), 6);

	// Draw Progress Bar background track
	XSetForeground(ctx->display, ctx->gc, 0x00444444);
	XFillRectangle(ctx->display, ctx->win, ctx->gc, 0, win_h - 50, win_w, 5);

	// Calculate filled timeline bar area
	double progress_ratio = (double)ctx->current_pts / (double)ctx->fmt_ctx->streams[ctx->video_stream_idx]->duration;
	if (progress_ratio > 1.0)
		progress_ratio = 1.0;

	int filled_width = (int)(progress_ratio * win_w);
	XSetForeground(ctx->display, ctx->gc, 0x00FF0000);
	XFillRectangle(ctx->display, ctx->win, ctx->gc, 0, win_h - 50, filled_width, 5);

	// Display Current Progress Timestamp vs Total Runtime
	double cur_sec = (ctx->current_pts
		* av_q2d(ctx->fmt_ctx->streams[ctx->video_stream_idx]->time_base));
	double tot_sec = (ctx->fmt_ctx->streams[ctx->video_stream_idx]->duration
		* av_q2d(ctx->fmt_ctx->streams[ctx->video_stream_idx]->time_base));
	if (cur_sec < 0) cur_sec = 0;
	if (tot_sec < 0) tot_sec = 0;

	int cur_min = (int)cur_sec / 60;
	int cur_hr = cur_min / 60;
	cur_min %= 60;
	int cur_s = (int)cur_sec % 60;

	int tot_min = (int)tot_sec / 60;
	int tot_hr = tot_min / 60;
	tot_min %= 60;
	int tot_s = (int)tot_sec % 60;

	char time_str[64];
	snprintf(time_str, sizeof(time_str), (tot_hr > 0 ? "%02d:%02d:%02d / %02d:%02d:%02d" : "%c%02d:%02d /%c%02d:%02d"),
		(tot_hr > 0 ? cur_hr : ' '), cur_min, cur_s, (tot_hr > 0 ? cur_hr : ' '), tot_min, tot_s);

	XSetForeground(ctx->display, ctx->gc, 0x00FFFFFF);
	XDrawString(ctx->display, ctx->win, ctx->gc, 10, win_h - 55, time_str, strlen(time_str));

	XSetForeground(ctx->display, ctx->gc, 0x333333);
	XFillRectangle(ctx->display, ctx->win, ctx->gc, win_w - 50, win_h - 28, 25, 15);
	XSetForeground(ctx->display, ctx->gc, 0xFFFFFF);
	XDrawRectangle(ctx->display, ctx->win, ctx->gc, win_w - 50, win_h - 28, 25, 15);
	XDrawString(ctx->display, ctx->win, ctx->gc, (win_w - 50) + 5, (win_h - 28) + 10, "pip", strlen("pip"));
	XFlush(ctx->display);
	XUnlockDisplay(ctx->display);
}

/* Handles mouse movements and button click events on video overlay */
static void media_overlay_cb(Widget w, XtPointer client_data, XEvent *event, Boolean *dispatch) {
	ats_video_info *ctx = (ats_video_info *)client_data;

	pthread_mutex_lock(&ctx->media_mutex);
	if (event->type == ConfigureNotify) {
		int w = event->xconfigure.width;
		int h = event->xconfigure.height;
		if (ctx->canvas_w == w && ctx->canvas_h == h && ctx->ximage != NULL) {
			pthread_mutex_unlock(&ctx->media_mutex);
			return;
		}

		ctx->canvas_w = w;
		ctx->canvas_h = h;
		if (ctx->ximage) {
			XDestroyImage(ctx->ximage);
			ctx->ximage = NULL;
		}

		if (ctx->sws_ctx) {
			sws_freeContext(ctx->sws_ctx);
			ctx->sws_ctx = NULL;
		}
	} else if (event->type == Expose && ctx->ximage) {
		XLockDisplay(ctx->display);
		XPutImage(ctx->display, ctx->win, ctx->gc, ctx->ximage, 0, 0, 0, 0, ctx->canvas_w, ctx->canvas_h);
		XFlush(ctx->display);
		XUnlockDisplay(ctx->display);
	} else if (event->type == KeyPress) {
		KeySym key = XLookupKeysym(&event->xkey, 0);
		if (!ctx->is_fullscreen && key == XK_Home)
			toggle_fullscreen(ctx);
		else if (key == XK_Escape)
			ctx->state = STATE_STOPPED;
	} else if (event->type == ButtonPress) {
		Dimension width, height;
		XtVaGetValues(w,
			XtNwidth, &width,
			XtNheight, &height, NULL);

		int ctrl_y = height - OVERLAY_HEIGHT;
		overlay_rect btn_play_pause = {10,  ctrl_y, 40, 25};
		overlay_rect btn_vol = {65, ctrl_y, 40, 25};
		overlay_rect btn_pip = {width - 50,  height - 28, 25, 15};
		overlay_rect progress_bar = {0, height - PROGRESS_HEIGHT, width, 5};
		int mx = event->xbutton.x;
		int my = event->xbutton.y;

		if (media_click_check(btn_play_pause, mx, my)) {
			ctx->state = (ctx->state == STATE_PLAYING) ? STATE_PAUSED : STATE_PLAYING;
		} else if (media_click_check(btn_vol, mx, my)) {
			ctx->volume = (float)(mx - btn_vol.x) / btn_vol.w;
			if (ctx->volume < 0.0) ctx->volume = 0.0;
			if (ctx->volume > 1.0) ctx->volume = 1.0;
		} else if (media_click_check(btn_pip, mx, my)) {
			toggle_pip(ctx);
		} else if (media_click_check(progress_bar, mx, my)) {
			double click_percentage = (double)mx / (double)progress_bar.w;
			AVRational video_tb = ctx->fmt_ctx->streams[ctx->video_stream_idx]->time_base;
			double seek_pts = click_percentage * (double)ctx->fmt_ctx->streams[ctx->video_stream_idx]->duration;
			ctx->seek_target_pts = (int64_t)(seek_pts / av_q2d(video_tb));
			ctx->seek_requested = true;
		}
	}
	pthread_mutex_unlock(&ctx->media_mutex);
}

static void media_hide_cb(XtPointer client_data, XtIntervalId *id) {
	ats_video_info *ctx = (ats_video_info *)client_data;
	pthread_mutex_lock(&ctx->media_mutex);
	if (ctx->controls_visible && ctx->state == STATE_PLAYING) {
		ctx->controls_visible = 0;
	}
	ctx->hide_timeout_id = 0;
	pthread_mutex_unlock(&ctx->media_mutex);
}

static void media_hide_timer(ats_video_info *ctx) {
	if (ctx->hide_timeout_id)
		XtRemoveTimeOut(ctx->hide_timeout_id);

	ctx->hide_timeout_id = XtAppAddTimeOut(XtWidgetToApplicationContext(ctx->media_area),
		HIDE_TIMEOUT_MS, media_hide_cb, ctx);
}

static void media_motion_cb(Widget w, XtPointer client_data, XEvent *event, Boolean *dispatch) {
	ats_video_info *ctx = (ats_video_info *)client_data;
	pthread_mutex_lock(&ctx->media_mutex);
	if (event->type == MotionNotify) {
		if (!ctx->controls_visible)
			ctx->controls_visible = 1;

		media_hide_timer(ctx);
	} else if (event->type == LeaveNotify) {
		if (ctx->controls_visible && ctx->state == STATE_PLAYING)
			ctx->controls_visible = 0;

		ctx->hide_timeout_id = 0;
	}
	ctx->last_move_time = ats_time();
	pthread_mutex_unlock(&ctx->media_mutex);
}

static void pip_active_indicator(ats_video_info *ctx) {
	XSetForeground(ctx->display, ctx->gc, 0x000000);
	XFillRectangle(ctx->display, ctx->win, ctx->gc, 0, 0, ctx->canvas_w, ctx->canvas_h);

	int symbol_w = ctx->canvas_w / 6;
	int symbol_h = ctx->canvas_h / 6;
	if (symbol_w < 60) symbol_w = 60;
	if (symbol_h < 40) symbol_h = 40;

	int center_x = ctx->canvas_w / 2;
	int center_y = ctx->canvas_h / 2;

	XSetForeground(ctx->display, ctx->gc, 0x00FF00);
	XSetLineAttributes(ctx->display, ctx->gc, 3, LineSolid, CapRound, JoinRound);

	// Outer Indicator Outline
	XDrawRectangle(ctx->display, ctx->win, ctx->gc, center_x - symbol_w,
		center_y - symbol_h, symbol_w * 2, symbol_h * 2);

	// Dual Display Indicator
	int inner_w = symbol_w / 2;
	int inner_h = symbol_h / 2;
	XFillRectangle(ctx->display, ctx->win, ctx->gc, center_x + symbol_w / 4,
		center_y + symbol_h / 4, inner_w, inner_h);
	XSetForeground(ctx->display, ctx->gc, 0x000000);
	XDrawRectangle(ctx->display, ctx->win, ctx->gc, center_x + symbol_w / 4,
		center_y + symbol_h / 4, inner_w, inner_h);
	XSetForeground(ctx->display, ctx->gc, 0x00FF00);
	const char *pip_msg = "PiP Active";
	XDrawString(ctx->display, ctx->win, ctx->gc, center_x - 85, center_y + symbol_h + 30,
		pip_msg, strlen(pip_msg));
}

static void *media_thread(void *arg) {
	ats_video_info *ctx = (ats_video_info *)arg;
	AVPacket *packet = av_packet_alloc();
	AVFrame *frame = av_frame_alloc();
	AVFrame *frame_rgb = av_frame_alloc();
	uint8_t *audio_buf = NULL;
	float current_vol;
	int target_video_h, render_w, render_h, win_w, win_h;
	if (ctx->has_audio)
		audio_buf = malloc(MAX_AUDIO_FRAME_SIZE);

	while (!ctx->quit) {
		pthread_mutex_lock(&ctx->media_mutex);
		PlayState run_state = ctx->state;
		pthread_mutex_unlock(&ctx->media_mutex);

		if (run_state == STATE_STOPPED) {
			media_overlay_controls(ctx, ctx->canvas_w, ctx->canvas_h);
			usleep(10000);
			continue;
		}

		if (run_state == STATE_PAUSED) {
			if (ctx->controls_visible)
				media_overlay_controls(ctx, ctx->canvas_w, ctx->canvas_h);

			usleep(20000);
			continue;
		}

		pthread_mutex_lock(&ctx->media_mutex);
		if (ctx->seek_requested) {
			// Precise seek within container stream context
			av_seek_frame(ctx->fmt_ctx, ctx->video_stream_idx, (ctx->seek_target_pts * AV_TIME_BASE), AVSEEK_FLAG_BACKWARD);
			avcodec_flush_buffers(ctx->video_ctx);
			if (ctx->audio_ctx)
				avcodec_flush_buffers(ctx->audio_ctx);

			if (ctx->pcm_handle)
				snd_pcm_prepare(ctx->pcm_handle);

			ctx->audio_clock = ctx->seek_target_pts;
			ctx->seek_requested = false;
		}
		current_vol = ctx->volume;
		pthread_mutex_unlock(&ctx->media_mutex);

		if (av_read_frame(ctx->fmt_ctx, packet) >= 0) {
			if (packet->stream_index == ctx->video_stream_idx) {
				if (avcodec_send_packet(ctx->video_ctx, packet) == 0) {
					while (avcodec_receive_frame(ctx->video_ctx, frame) == 0) {
						AVRational fr = ctx->fmt_ctx->streams[ctx->video_stream_idx]->avg_frame_rate;
						double frame_delay_us = (fr.num > 0 && fr.den > 0) ? (double)fr.den / fr.num * 1e6 : 30000;
						ctx->current_pts = frame->pts != AV_NOPTS_VALUE ? frame->pts : frame->pkt_dts;
						pthread_mutex_lock(&ctx->media_mutex);
						if (ctx->pip_mode && ctx->pip_win) {
							// Scale video to window dimensions
							XWindowAttributes pip_attr;
							XGetWindowAttributes(ctx->display, ctx->pip_win, &pip_attr);
							if (ctx->is_fullscreen)
								win_w = ctx->saved_w, win_h = ctx->saved_h;
							else
								win_w = pip_attr.width, win_h = pip_attr.height;

							target_video_h = win_h, render_w = win_w, render_h = target_video_h;
							double win_ar = (double)win_w / target_video_h;
							double aspect_ratio = ctx->is_fullscreen ? ctx->full_aspect : ctx->aspect_ratio;
							if (win_ar > aspect_ratio) {
								render_w = (int)(target_video_h * aspect_ratio);
							} else {
								render_h = (int)(win_w / aspect_ratio);
							}

							ctx->pip_w = win_w;
							ctx->pip_h = win_h;
							if (!ctx->pip_sws_ctx && ctx->pip_w > 0) {
								int unaligned_stride = render_w * 4;
								int aligned_stride = (unaligned_stride + 31) & ~31;
								size_t total_buffer_bytes = aligned_stride * render_h;
								ctx->pip_image_buffer = av_mallocz(total_buffer_bytes);
								ctx->pip_ximage = XCreateImage(ctx->display, DefaultVisual(ctx->display, DefaultScreen(ctx->display)), DefaultDepth(ctx->display, DefaultScreen(ctx->display)), ZPixmap, 0,
									(char *)ctx->pip_image_buffer, render_w, render_h, 32, aligned_stride);

								frame_rgb->data[0] = ctx->pip_image_buffer;
								frame_rgb->linesize[0] = aligned_stride;

								ctx->pip_sws_ctx = sws_getCachedContext(ctx->pip_sws_ctx, ctx->canvas_w, ctx->canvas_h,
									ctx->video_ctx->pix_fmt, render_w, render_h, AV_PIX_FMT_BGRA,
									SWS_BILINEAR, NULL, NULL, NULL);
							}

							XLockDisplay(ctx->display);
							if (ctx->pip_sws_ctx && ctx->pip_ximage) {
								sws_scale(ctx->pip_sws_ctx,
									(const uint8_t *const *)frame->data,
									frame->linesize,
									0,
									(ctx->is_fullscreen ? target_video_h : ctx->canvas_h),
									frame_rgb->data,
									frame_rgb->linesize);
								XPutImage(ctx->display, ctx->pip_win, ctx->pip_gc, ctx->pip_ximage,
									0, 0, 0, 0, render_w, render_h);
							}

							pip_active_indicator(ctx);
							XFlush(ctx->display);
							XUnlockDisplay(ctx->display);
						} else {
							if (!ctx->sws_ctx && ctx->canvas_w > 0) {
								ctx->sws_ctx = sws_getContext(
									ctx->video_ctx->width, ctx->video_ctx->height, ctx->video_ctx->pix_fmt,
									ctx->canvas_w, ctx->canvas_h, AV_PIX_FMT_BGRA,
									SWS_BILINEAR, NULL, NULL, NULL
								);

								ctx->image_buffer = malloc(ctx->canvas_w * ctx->canvas_h * 4);
								ctx->ximage = XCreateImage(ctx->display, CopyFromParent, 24, ZPixmap, 0,
									(char *)ctx->image_buffer, ctx->canvas_w, ctx->canvas_h, 32, 0);
							}

							if (ctx->sws_ctx && ctx->ximage) {
								uint8_t *dst_data[4] = {ctx->image_buffer, NULL, NULL, NULL};
								int dst_linesize[4] = {ctx->canvas_w * 4, 0, 0, 0};
								sws_scale(ctx->sws_ctx, (const uint8_t *const *)frame->data, frame->linesize,
									0, frame->height, dst_data, dst_linesize);
								ats_render_image(ctx->media_area, ctx->gc, ctx->ximage,
									ctx->canvas_w, ctx->canvas_h, 0, 0, 0, NULL);
							}
						}
						pthread_mutex_unlock(&ctx->media_mutex);
						if (ctx->controls_visible)
							media_overlay_controls(ctx, ctx->canvas_w, ctx->canvas_h);
						usleep((useconds_t)frame_delay_us);
					}
				}
			} else if (ctx->has_audio && ctx->pcm_handle && audio_buf && packet->stream_index == ctx->audio_stream_idx) {
				if (avcodec_send_packet(ctx->audio_ctx, packet) >= 0) {
					AVFrame *a_frame = av_frame_alloc();
					while (avcodec_receive_frame(ctx->audio_ctx, a_frame) >= 0) {
						int i, samples = swr_convert(ctx->swr_ctx, &audio_buf, MAX_AUDIO_FRAME_SIZE / 2,
							(const uint8_t **)a_frame->data, a_frame->nb_samples);
						int out_size = samples * ctx->audio_ctx->channels * 2;

						int16_t *samples_ptr = (int16_t *)audio_buf;
						for (i = 0; i < out_size / 2; i++) {
							int32_t sample = samples_ptr[i] * current_vol;
							samples_ptr[i] = (sample > 32767) ? 32767 : ((sample < -32768) ? -32768 : sample);
						}

						// Write decoded audio chunks into the ALSA buffer
						snd_pcm_sframes_t written = snd_pcm_writei(ctx->pcm_handle, audio_buf, samples);
						// Recover from underruns if audio buffers fall behind decoding pacing
						if (written == -EPIPE)
							snd_pcm_prepare(ctx->pcm_handle);

						if (a_frame->pts != AV_NOPTS_VALUE)
							ctx->audio_clock = a_frame->pts * av_q2d(ctx->fmt_ctx->streams[ctx->audio_stream_idx]->time_base);
					}
					av_frame_free(&a_frame);
				}
			}
			av_packet_unref(packet);
		} else {
			// End of stream tracking force auto-reset
			av_seek_frame(ctx->fmt_ctx, ctx->video_stream_idx, 0, AVSEEK_FLAG_BACKWARD);
			pthread_mutex_lock(&ctx->media_mutex);
			ctx->state = STATE_STOPPED;
			pthread_mutex_unlock(&ctx->media_mutex);
		}
	}

	if (ctx->has_audio && audio_buf)
		free(audio_buf);

	av_frame_free(&frame);
	av_frame_free(&frame_rgb);
	av_packet_free(&packet);
	free_video(ctx);
	return NULL;
}

static bool process_media(ats_video_info *ctx, const char *url) {
	snprintf(ctx->filepath, sizeof(ctx->filepath), "%s", url);
	ctx->state = STATE_STOPPED;
	ctx->volume = 0.50;
	ctx->has_audio = false;
	pthread_mutex_init(&ctx->media_mutex, NULL);

	if (avformat_open_input(&ctx->fmt_ctx, ctx->filepath, NULL, NULL) < 0
		|| avformat_find_stream_info(ctx->fmt_ctx, NULL) < 0)
		return false;

	unsigned int i;
	ctx->video_stream_idx = -1;
	for (i = 0; i < ctx->fmt_ctx->nb_streams; i++) {
		if (ctx->fmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
			ctx->video_stream_idx = i;
			break;
		}
	}

	if (ctx->video_stream_idx != -1) {
		const AVCodec *codec = avcodec_find_decoder(ctx->fmt_ctx->streams[ctx->video_stream_idx]->codecpar->codec_id);
		ctx->video_ctx = avcodec_alloc_context3(codec);
		avcodec_parameters_to_context(ctx->video_ctx, ctx->fmt_ctx->streams[ctx->video_stream_idx]->codecpar);
		avcodec_open2(ctx->video_ctx, codec, NULL);

		ctx->canvas_w = ctx->fmt_ctx->streams[ctx->video_stream_idx]->codecpar->width;
		ctx->canvas_h = ctx->fmt_ctx->streams[ctx->video_stream_idx]->codecpar->height;
		ctx->meta = av_dict_get(ctx->fmt_ctx->metadata, "title", NULL, 0);
		ctx->pip_w = ctx->canvas_w /2;
		ctx->pip_h = ctx->canvas_h /2;
		ctx->aspect_ratio = (double)ctx->canvas_w / ctx->canvas_h;
	}

	ctx->audio_stream_idx = av_find_best_stream(ctx->fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
	if (ctx->audio_stream_idx >= 0) {
		const AVCodec *a_codec = avcodec_find_decoder(ctx->fmt_ctx->streams[ctx->audio_stream_idx]->codecpar->codec_id);
		ctx->audio_ctx = avcodec_alloc_context3(a_codec);
		avcodec_parameters_to_context(ctx->audio_ctx, ctx->fmt_ctx->streams[ctx->audio_stream_idx]->codecpar);
		if (avcodec_open2(ctx->audio_ctx, a_codec, NULL) >= 0) {
			if (!init_alsa(ctx, ctx->audio_ctx->sample_rate, ctx->audio_ctx->channels)) {
				ctx->swr_ctx = swr_alloc_set_opts(NULL,
					ctx->audio_ctx->channel_layout, AV_SAMPLE_FMT_S16, ctx->audio_ctx->sample_rate,
					ctx->audio_ctx->channel_layout, ctx->audio_ctx->sample_fmt, ctx->audio_ctx->sample_rate, 0, NULL);
				swr_init(ctx->swr_ctx);
			}
		}
	}

	pthread_create(&ctx->media_tid, NULL, media_thread, ctx);
	return true;
}

static void volume_up_cb(Widget w, XtPointer client_data, XtPointer call_data) {
	ats_video_info *ctx = (ats_video_info *)client_data;
	pthread_mutex_lock(&ctx->media_mutex);
	ctx->volume += 0.1f;
	if (ctx->volume > 1.0f)
		ctx->volume = 1.0f;
	pthread_mutex_unlock(&ctx->media_mutex);
}

static void volume_down_cb(Widget w, XtPointer client_data, XtPointer call_data) {
	ats_video_info *ctx = (ats_video_info *)client_data;
	pthread_mutex_lock(&ctx->media_mutex);
	ctx->volume -= 0.1f;
	if (ctx->volume < 0.0f)
		ctx->volume = 0.0f;
	pthread_mutex_unlock(&ctx->media_mutex);
}

static int load_video(ats_t *ui, ats_wnd on, const char *url) {
	bool is_ready = false;
	if (url && process_media(&ui->av, url)) {
		ui->use_icon = movie;
		ui->av.video_set = true;
		if (!on) {
			is_ready = ats_window(ui, ui->av.filepath, ui->av.canvas_w, ui->av.canvas_h, false);
			ui->av.display = XtDisplay(ui->topLevel);
			ui->av.top_area = ui->topLevel;
			ui->av.top_win = ui->win;
		} else {
			is_ready = true;
			ui->topLevel = on;
			ui->av.top_area = ui->topLevel;
			ui->av.top_win = XtWindow(ui->topLevel);
			ui->av.display = XtDisplay(ui->topLevel);
			ui->dpy = ui->av.display;
			ui->win = ui->av.top_win;
		}

		if (is_ready) {
			ui->av.media_area = ats_videocanvas_set(&ui->av, ui->topLevel,
				ui->av.canvas_w, ui->av.canvas_h, media_overlay_cb);
			ats_keybutton_set(ui->av.media_area, media_overlay_cb, &ui->av);
			ats_exposure_set(ui->av.media_area, media_overlay_cb, &ui->av);
			ats_motion_set(ui->av.media_area, media_motion_cb, &ui->av);
			ui->app->app_data = ui->av.media_area;
			ui->app->name = ui->av.meta ? ui->av.meta->value : basename((char *)url);
			ui->app->wnd = ui->topLevel;
			ui->av.win = XtWindow(ui->av.media_area);
			ui->av.gc = XCreateGC(ui->av.display, ui->av.win, 0, NULL);
			ui->av.controls_visible = 1;
			media_hide_timer(&ui->av);
		}
	}

	if (!url)
		fprintf(stderr, "No video provided!\n");

	return is_ready;
}
