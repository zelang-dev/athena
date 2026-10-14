#include <athena.h>

#include "../share/pixmaps/movie.xpm"

// Cleanup after thread loop exits
static void free_video(ats_video_info *ctx) {
	// Pipeline Clean Down Procedures
	if (ctx->pcm_handle) {
		snd_pcm_drain(ctx->pcm_handle);
		snd_pcm_close(ctx->pcm_handle);
		ctx->pcm_handle = NULL;
	}

	if (ctx->sws_ctx)
		sws_freeContext(ctx->sws_ctx);

	if (ctx->swr_ctx)
		swr_free(&ctx->swr_ctx);

	avcodec_free_context(&ctx->video_ctx);
	if (ctx->audio_ctx)
		avcodec_free_context(&ctx->audio_ctx);

	avformat_close_input(&ctx->fmt_ctx);
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

	return 0;
}

// Safely reconstruct canvas size on-the-fly
static void resize_canvas(ats_video_info *ctx, int w, int h) {
	pthread_mutex_lock(&ctx->media_mutex);
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

	ctx->image_buffer = malloc(w * h * 4);
	ctx->ximage = XCreateImage(ctx->display, CopyFromParent, 24, ZPixmap, 0,
		(char *)ctx->image_buffer, w, h, 32, 0);
	pthread_mutex_unlock(&ctx->media_mutex);
}

static void canvas_resize_handler(Widget w, XtPointer client_data, XEvent *event, Boolean *dispatched) {
	ats_video_info *ctx = (ats_video_info *)client_data;
	if (event->type == ConfigureNotify) {
		int new_w = event->xconfigure.width;
		int new_h = event->xconfigure.height;
		resize_canvas(ctx, new_w, new_h);
	}
}

static void *media_thread(void *arg) {
	ats_video_info *ctx = (ats_video_info *)arg;
	AVPacket *packet = av_packet_alloc();
	AVFrame *frame = av_frame_alloc();
	uint8_t *audio_buf = malloc(MAX_AUDIO_FRAME_SIZE);
	float current_vol, scroll_pos = 0.0;

	while (!ctx->quit) {
		pthread_mutex_lock(&ctx->media_mutex);
		PlayState run_state = ctx->state;
		pthread_mutex_unlock(&ctx->media_mutex);

		if (run_state == STATE_STOPPED) {
			av_seek_frame(ctx->fmt_ctx, ctx->video_stream_idx, 0, AVSEEK_FLAG_BACKWARD);
			usleep(20000);
			continue;
		}
		if (run_state == STATE_PAUSED) {
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
				AVRational fr = ctx->fmt_ctx->streams[ctx->video_stream_idx]->avg_frame_rate;
				double frame_delay_us = (fr.num > 0 && fr.den > 0) ? (double)fr.den / fr.num * 1e6 : 30000;
				if (avcodec_send_packet(ctx->video_ctx, packet) == 0) {
					while (avcodec_receive_frame(ctx->video_ctx, frame) == 0) {
						pthread_mutex_lock(&ctx->media_mutex);
						// Guard scale process if context isn't ready or matching bounds
						if (!ctx->sws_ctx && ctx->canvas_w > 0) {
							ctx->sws_ctx = sws_getContext(
								frame->width, frame->height, frame->format,
								ctx->canvas_w, ctx->canvas_h, AV_PIX_FMT_BGRA,
								SWS_FAST_BILINEAR, NULL, NULL, NULL
							);
						}

						if (ctx->sws_ctx && ctx->ximage) {
							uint8_t *dst_data[4] = {ctx->image_buffer, NULL, NULL, NULL};
							int dst_linesize[4] = {ctx->canvas_w * 4, 0, 0, 0};

							sws_scale(ctx->sws_ctx, (const uint8_t *const *)frame->data, frame->linesize,
								0, frame->height, dst_data, dst_linesize);

							double current_sec = frame->pts * av_q2d(ctx->fmt_ctx->streams[ctx->video_stream_idx]->time_base);
							scroll_pos = (ctx->duration > 0) ? (float)(current_sec / ctx->duration) : 0.0f;
							if (scroll_pos > 1.0f)
								scroll_pos = 1.0f;

						}
						pthread_mutex_unlock(&ctx->media_mutex);

						// Render `image` adding `text` overlay directly over X11 graphics buffer
						if (!ats_rendering_video(ctx->media_area, ctx->ximage, ctx->canvas_w, ctx->canvas_h,
							5, ctx->canvas_h - 5, 0xFFFFFF, NULL)) {
							usleep((useconds_t)frame_delay_us);
						}

						XawScrollbarSetThumb(ctx->progress_bar, scroll_pos, -1.0);
					}
				}
			} else if (packet->stream_index == ctx->audio_stream_idx && ctx->pcm_handle) {
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
			pthread_mutex_lock(&ctx->media_mutex);
			ctx->state = STATE_STOPPED;
			pthread_mutex_unlock(&ctx->media_mutex);
			XtVaSetValues(ctx->play_pause_label, XtNlabel, "Play  ", NULL);
		}
	}

	free(audio_buf);
	av_frame_free(&frame);
	av_packet_free(&packet);
	free_video(ctx);
	return NULL;
}

static bool process_media(ats_video_info *ctx, const char *url) {
	snprintf(ctx->filepath, sizeof(ctx->filepath), "%s", url);
	ctx->state = STATE_STOPPED;
	ctx->volume = 0.50;
	pthread_mutex_init(&ctx->media_mutex, NULL);

	if (avformat_open_input(&ctx->fmt_ctx, url, NULL, NULL) < 0
		|| avformat_find_stream_info(ctx->fmt_ctx, NULL) < 0)
		return false;

	ctx->video_stream_idx = av_find_best_stream(ctx->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
	ctx->audio_stream_idx = av_find_best_stream(ctx->fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);

	AVStream *st = ctx->fmt_ctx->streams[ctx->video_stream_idx];
	const AVCodec *codec = avcodec_find_decoder(st->codecpar->codec_id);
	ctx->video_ctx = avcodec_alloc_context3(codec);
	avcodec_parameters_to_context(ctx->video_ctx, st->codecpar);
	avcodec_open2(ctx->video_ctx, codec, NULL);

	if (ctx->audio_stream_idx >= 0) {
		const AVCodec *a_codec = avcodec_find_decoder(ctx->fmt_ctx->streams[ctx->audio_stream_idx]->codecpar->codec_id);
		ctx->audio_ctx = avcodec_alloc_context3(a_codec);
		avcodec_parameters_to_context(ctx->audio_ctx, ctx->fmt_ctx->streams[ctx->audio_stream_idx]->codecpar);
		avcodec_open2(ctx->audio_ctx, a_codec, NULL);

		init_alsa(ctx, ctx->audio_ctx->sample_rate, ctx->audio_ctx->channels);
		ctx->swr_ctx = swr_alloc_set_opts(NULL,
			ctx->audio_ctx->channel_layout, AV_SAMPLE_FMT_S16, ctx->audio_ctx->sample_rate,
			ctx->audio_ctx->channel_layout, ctx->audio_ctx->sample_fmt, ctx->audio_ctx->sample_rate, 0, NULL);
		swr_init(ctx->swr_ctx);
	}

	ctx->duration = (double)ctx->fmt_ctx->duration / AV_TIME_BASE;
	ctx->canvas_w = ctx->video_ctx->width;
	ctx->canvas_h = ctx->video_ctx->height;

	pthread_create(&ctx->media_tid, NULL, media_thread, ctx);
	return true;
}

static void cb_progress_jump(Widget w, XtPointer client_data, XtPointer call_data) {
	ats_video_info *ctx = (ats_video_info *)client_data;
	float percent = *(float *)call_data;

	pthread_mutex_lock(&ctx->media_mutex);
	if (ctx->duration > 0) {
		AVRational video_tb = ctx->fmt_ctx->streams[ctx->video_stream_idx]->time_base;
		double target_secs = percent * ctx->duration;
		ctx->seek_target_pts = (int64_t)(target_secs / av_q2d(video_tb));
		ctx->seek_requested = true;
	}
	pthread_mutex_unlock(&ctx->media_mutex);
}

static void play_pause_cb(Widget w, XtPointer client_data, XtPointer call_data) {
	ats_video_info *ctx = (ats_video_info *)client_data;
	pthread_mutex_lock(&ctx->media_mutex);
	if (ctx->state == STATE_STOPPED)
		ctx->seek_requested = true;

	ctx->state = ctx->state == STATE_PLAYING ? STATE_PAUSED : STATE_PLAYING;
	XtVaSetValues(ctx->play_pause_label, XtNlabel, (ctx->state == STATE_PLAYING ? "Pause " : "Play  "), NULL);
	pthread_mutex_unlock(&ctx->media_mutex);
	if (ctx->seek_requested)
		XtResizeWidget(ctx->media_area, ctx->canvas_w, ctx->canvas_h, 0);
}

static void stop_media_cb(Widget w, XtPointer client_data, XtPointer call_data) {
	ats_video_info *ctx = (ats_video_info *)client_data;
	pthread_mutex_lock(&ctx->media_mutex);
	ctx->seek_requested = true;
	ctx->state = STATE_STOPPED;
	ctx->progress = 0.0;
	XtVaSetValues(ctx->play_pause_label, XtNlabel, "Play  ", NULL);
	XawScrollbarSetThumb(ctx->progress_bar, 0.0, -1.0);
	pthread_mutex_unlock(&ctx->media_mutex);
}

static void media_volume_cb(Widget w, XtPointer client_data, XtPointer call_data) {
	ats_video_info *ctx = (ats_video_info *)client_data;
	float position = (float)(intptr_t)call_data;
	pthread_mutex_lock(&ctx->media_mutex);
	ctx->volume = position;
	pthread_mutex_unlock(&ctx->media_mutex);
}

void volume_up_cb(Widget w, XtPointer client_data, XtPointer call_data) {
	ats_video_info *ctx = (ats_video_info *)client_data;
	pthread_mutex_lock(&ctx->media_mutex);
	ctx->volume += 0.1f;
	if (ctx->volume > 1.0f)
		ctx->volume = 1.0f;
	char buf[16];
	sprintf(buf, "Volume: %d%%", (int)(ctx->volume * 100));
	XtVaSetValues(ctx->volume_bar, XtNlabel, buf, NULL);
	pthread_mutex_unlock(&ctx->media_mutex);
}

void volume_down_cb(Widget w, XtPointer client_data, XtPointer call_data) {
	ats_video_info *ctx = (ats_video_info *)client_data;
	pthread_mutex_lock(&ctx->media_mutex);
	ctx->volume -= 0.1f;
	if (ctx->volume < 0.0f)
		ctx->volume = 0.0f;
	char buf[16];
	sprintf(buf, "Volume: %d%%", (int)(ctx->volume * 100));
	XtVaSetValues(ctx->volume_bar, XtNlabel, buf, NULL);
	pthread_mutex_unlock(&ctx->media_mutex);
}

int load_video(ats_t *ui, const char *url) {
	if (!url) {
		fprintf(stderr, "No video provided!\n");
		return 0;
	}
	int argc = 0;
	char **argv = NULL;
	if (process_media(&ui->av, url)) {
		ui->use_icon = movie;
		ui->video_set = true;
		if (ats_window(ui, ui->av.filepath, (ui->av.canvas_w + 20), (ui->av.canvas_h + 60), false)) {
			Widget form = ats_formwindow_set(ui->topLevel, 0);
			Widget layout_wrapper = ats_videobox_set(form, NULL, NULL, ui->av.canvas_w, ui->av.canvas_h);
			ui->av.media_area = ats_videocanvas_set(&ui->av, layout_wrapper, ui->av.canvas_w, ui->av.canvas_h,
				canvas_resize_handler);

			ui->av.progress_bar = XtVaCreateManagedWidget("progress", scrollbarWidgetClass, layout_wrapper,
				XtNorientation, XtorientHorizontal, XtNwidth, ui->av.canvas_w, XtNheight, 12,
				XtNfromVert, ui->av.media_area, NULL);
			XtAddCallback(ui->av.progress_bar, XtNjumpProc, cb_progress_jump, &ui->av);

			ui->av.play_pause_label = XtVaCreateManagedWidget("button", commandWidgetClass, layout_wrapper,
				XtNfromVert, ui->av.progress_bar, XtNlabel, "Play ", NULL);
			XtAddCallback(ui->av.play_pause_label, XtNcallback, play_pause_cb, &ui->av);
			Widget stop_btn = XtVaCreateManagedWidget("button", commandWidgetClass, layout_wrapper,
				XtNfromVert, ui->av.progress_bar, XtNfromHoriz, ui->av.play_pause_label, XtNlabel, "Stop", NULL);
			XtAddCallback(stop_btn, XtNcallback, stop_media_cb, &ui->av);

			ui->av.volume_bar = XtVaCreateManagedWidget("Vol", scrollbarWidgetClass, layout_wrapper,
				XtNorientation, XtorientHorizontal, XtNwidth, 70, XtNfromVert, ui->av.progress_bar, XtNfromHoriz, stop_btn, NULL);
			XtAddCallback(ui->av.volume_bar, XtNjumpProc, media_volume_cb, &ui->av);
			XtAddCallback(ui->av.volume_bar, XtNscrollProc, media_volume_cb, &ui->av);
			XtAddCallback(ui->av.volume_bar, XtNthumbProc, media_volume_cb, &ui->av);
			XawScrollbarSetThumb(ui->av.volume_bar, ui->av.volume, 0.05);

			ui->app->app_data = layout_wrapper;
			ui->app->name = basename((char *)url);
			ui->app->wnd = form;
			ui->av.display = ui->dpy;
			return 1;
		}
	}


	return 0;
}
