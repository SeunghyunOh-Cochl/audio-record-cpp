/***
  This file is part of PulseAudio.

  Copyright 2004-2006 Lennart Poettering
  Copyright 2006 Pierre Ossman <ossman@cendio.se> for Cendio AB

  PulseAudio is free software; you can redistribute it and/or modify
  it under the terms of the GNU Lesser General Public License as published
  by the Free Software Foundation; either version 2.1 of the License,
  or (at your option) any later version.

  PulseAudio is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
  General Public License for more details.

  You should have received a copy of the GNU Lesser General Public License
  along with PulseAudio; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
  USA.

  g++ pulseaudio-stream-example.cc -o pulseaudio-stream-example -lm -std=c++11 -ldl -lstdc++ -lpulse -lpulse-simple
***/

// #include <pulse/i18n.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <iostream>
#include <memory>
#include <pulse/pulseaudio.h>
#include <pulse/rtclock.h>

#include <signal.h>
#include <chrono>
#include <iostream>


#define SAMPLE_RATE 22050
#define BUF_SIZE (SAMPLE_RATE) / 2
#define CLEAR_LINE "\x1B[K"

// make_unique is an upcomming C++14 feature. So, we need to implement
// make_unique in C++11.
template <typename T, typename... Args>
std::unique_ptr<T> make_unique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

static enum { RECORD, PLAYBACK } mode = RECORD;

static pa_io_event* stdio_event = NULL;
static pa_mainloop_api *mainloop_api = NULL;
static pa_stream *stream = NULL;

static void *buffer = NULL;
static size_t buffer_length = 0, buffer_index = 0;

static pa_context *context = NULL;
static int verbose = 0;
static pa_stream_flags_t flags;

// static char *stream_name = NULL, *client_name = NULL, *device = NULL;

static std::unique_ptr<std::string> stream_name, client_name, device;


static pa_sample_spec sample_spec = {
    .format = PA_SAMPLE_S16LE,
    .rate = SAMPLE_RATE,
    .channels = 1
};
static size_t latency = 0, process_time=0;


/* A shortcut for terminating the application */
static void quit(int ret) {
    assert(mainloop_api);
    mainloop_api->quit(mainloop_api, ret);
}

/* UNIX signal to quit recieved */
static void exit_signal_callback(pa_mainloop_api*m, pa_signal_event *e, int sig, void *userdata) {
    if (verbose)
        fprintf(stderr, ("Got signal, exiting.\n"));
    quit(0);
}

/* Connection draining complete */
static void context_drain_complete(pa_context*c, void *userdata) {
    pa_context_disconnect(c);
}

/* Stream draining complete */
static void stream_drain_complete(pa_stream*s, int success, void *userdata) {

    if (!success) {
        fprintf(stderr, ("Failed to drain stream: %s\n"), pa_strerror(pa_context_errno(context)));
        quit(1);
    }

    if (verbose)
        fprintf(stderr, ("Playback stream drained.\n"));

    pa_stream_disconnect(stream);
    pa_stream_unref(stream);
    stream = NULL;

    if (!pa_context_drain(context, context_drain_complete, NULL))
        pa_context_disconnect(context);
    else {
        if (verbose)
            fprintf(stderr, ("Draining connection to server.\n"));
    }
}

/* Write some data to the stream */
static void do_stream_write(size_t length) {
    size_t l;
    assert(length);

    if (!buffer || !buffer_length)
        return;

    l = length;
    if (l > buffer_length)
        l = buffer_length;

    if (pa_stream_write(stream, (uint8_t*) buffer + buffer_index, l, NULL, 0, PA_SEEK_RELATIVE) < 0) {
        fprintf(stderr, ("pa_stream_write() failed: %s\n"), pa_strerror(pa_context_errno(context)));
        quit(1);
        return;
    }

    buffer_length -= l;
    buffer_index += l;

    if (!buffer_length) {
        pa_xfree(buffer);
        buffer = NULL;
        buffer_index = buffer_length = 0;
    }
}

/* New data on STDIN **/
static void stdin_callback(pa_mainloop_api*a, pa_io_event *e, int fd, pa_io_event_flags_t f, void *userdata) {
    size_t l, w = 0;
    ssize_t r;

    assert(a == mainloop_api);
    assert(e);
    assert(stdio_event == e);

    if (buffer) {
        mainloop_api->io_enable(stdio_event, PA_IO_EVENT_NULL);
        return;
    }

    if (!stream || pa_stream_get_state(stream) != PA_STREAM_READY || !(l = w = pa_stream_writable_size(stream)))
        l = 4096;

    buffer = pa_xmalloc(l);

    if ((r = read(fd, buffer, l)) <= 0) {
        if (r == 0) {
            if (verbose)
                fprintf(stderr, ("Got EOF.\n"));

            if (stream) {
                pa_operation *o;

                if (!(o = pa_stream_drain(stream, stream_drain_complete, NULL))) {
                    fprintf(stderr, ("pa_stream_drain(): %s\n"), pa_strerror(pa_context_errno(context)));
                    quit(1);
                    return;
                }

                pa_operation_unref(o);
            } else
                quit(0);

        } else {
            fprintf(stderr, ("read() failed: %s\n"), strerror(errno));
            quit(1);
        }

        mainloop_api->io_free(stdio_event);
        stdio_event = NULL;
        return;
    }

    buffer_length = (uint32_t) r;
    buffer_index = 0;

    if (w)
        do_stream_write(w);
}

/* Some data may be written to STDOUT */
static void stdout_callback(pa_mainloop_api*a, pa_io_event *e, int fd, pa_io_event_flags_t f, void *userdata) {
    ssize_t r;

    assert(a == mainloop_api);
    assert(e);
    assert(stdio_event == e);

    if (!buffer) {
        mainloop_api->io_enable(stdio_event, PA_IO_EVENT_NULL);
        return;
    }

    assert(buffer_length);

    if ((r = write(fd, (uint8_t*) buffer+buffer_index, buffer_length)) <= 0) {
        fprintf(stderr, ("write() failed: %s\n"), strerror(errno));
        quit(1);

        mainloop_api->io_free(stdio_event);
        stdio_event = NULL;
        return;
    }

    buffer_length -= (uint32_t) r;
    buffer_index += (uint32_t) r;

    if (!buffer_length) {
        pa_xfree(buffer);
        buffer = NULL;
        buffer_length = buffer_index = 0;
    }
}

static void do_stream_process(){
  static uint8_t _buffer[BUF_SIZE];
  static uint8_t _buffer_length = NULL;
  static uint8_t _buffer_index = NULL;
}

static void stream_read_callback(pa_stream *s, size_t length, void *userdata){
    const void *data;
    assert(s);
    assert(length > 0);

    if (stdio_event)
      mainloop_api->io_enable(stdio_event, PA_IO_EVENT_OUTPUT);
    
    fprintf(stderr, "length %ld read \n", length);

    if (pa_stream_peek(s, &data, &length) < 0) {
        fprintf(stderr, ("pa_stream_peek() failed: %s\n"), pa_strerror(pa_context_errno(context)));
        return;
    }

    assert(data);
    assert(length > 0);

    if (buffer) {
        buffer = pa_xrealloc(buffer, buffer_length + length);
        memcpy((uint8_t*) buffer + buffer_length, data, length);
        buffer_length += length;
    } else {
        buffer = pa_xmalloc(length);
        memcpy(buffer, data, length);
        buffer_length = length;
        buffer_index = 0;
    }

    pa_stream_drop(s);

    do_stream_process();
}

/* This is called whenever the context status changes */
static void context_state_callback(pa_context *c, void *userdata) {
    assert(c);

    switch (pa_context_get_state(c)) {
        case PA_CONTEXT_CONNECTING:
        case PA_CONTEXT_AUTHORIZING:
        case PA_CONTEXT_SETTING_NAME:
            break;

        case PA_CONTEXT_READY: {
            int r;
            pa_buffer_attr buffer_attr;

            assert(c);
            assert(!stream);

            if (verbose)
                fprintf(stderr, ("Connection established.%s \n"), CLEAR_LINE);

            if (!(stream = pa_stream_new(c, (char*) stream_name.get(), &sample_spec, NULL))) {
                fprintf(stderr, ("pa_stream_new() failed: %s\n"), pa_strerror(pa_context_errno(c)));
                goto fail;
            }

            // pa_stream_set_state_callback(stream, stream_state_callback, NULL);
            // pa_stream_set_write_callback(stream, stream_write_callback, NULL);
            pa_stream_set_read_callback(stream, stream_read_callback, NULL);
            // pa_stream_set_suspended_callback(stream, stream_suspended_callback, NULL);
            // pa_stream_set_moved_callback(stream, stream_moved_callback, NULL);
            // pa_stream_set_underflow_callback(stream, stream_underflow_callback, NULL);
            // pa_stream_set_overflow_callback(stream, stream_overflow_callback, NULL);
            // pa_stream_set_started_callback(stream, stream_started_callback, NULL);
            // pa_stream_set_event_callback(stream, stream_event_callback, NULL);
            // pa_stream_set_buffer_attr_callback(stream, stream_buffer_attr_callback, NULL);

            if (latency > 0) {
                memset(&buffer_attr, 0, sizeof(buffer_attr));
                buffer_attr.tlength = (uint32_t) latency;
                buffer_attr.minreq = (uint32_t) process_time;
                buffer_attr.maxlength = (uint32_t) -1;
                buffer_attr.prebuf = (uint32_t) -1;
                buffer_attr.fragsize = (uint32_t) latency;
                flags = static_cast<pa_stream_flags_t>(flags | PA_STREAM_ADJUST_LATENCY);
            }

            if ((r = pa_stream_connect_record(stream, (char*) device.get(), latency > 0 ? &buffer_attr : NULL, flags)) < 0) {
                fprintf(stderr, ("pa_stream_connect_record() failed: %s\n"), pa_strerror(pa_context_errno(c)));
                goto fail;
            }
            break;
        }

        case PA_CONTEXT_TERMINATED:
            quit(0);
            break;

        case PA_CONTEXT_FAILED:
        default:
            fprintf(stderr, ("Connection failure: %s\n"), pa_strerror(pa_context_errno(c)));
            goto fail;
    }

    return;

fail:
    quit(1);
}

// To run this example, install pulseaudio on your machine
// sudo apt-get install -y libpulse-dev
// Make sure pulseaudio is set on a valid input
// -> $ pacmd list-sources | grep -e 'index:' -e device.string -e 'name:'
// To change the default source -> $ pacmd set-default-source "SOURCE_NAME"
// It's also very common that pulse audio is not starting correctly.
// pulseaudio -k #kill the process just in case
// pulseaudio -D #start it again
int main(int argc, char *argv[]) {
  int ret = 1, r, c;

  pa_mainloop* m = NULL;
  char *bn, *server = NULL;
  int error;
  

  stream_name = make_unique<std::string>("stream");
  client_name = make_unique<std::string>("client");

  mode = RECORD;

  if (!pa_sample_spec_valid(&sample_spec)) {
      fprintf(stderr, ("Invalid sample specification\n"));
      goto quit;
  }

  /* Set up a new main loop */
  printf("mainloop setting...\n");
  if (!(m = pa_mainloop_new())) {
      fprintf(stderr, ("pa_mainloop_new() failed.\n"));
      goto quit;
  }

  mainloop_api = pa_mainloop_get_api(m);

  r = pa_signal_init(mainloop_api);
  assert(r == 0);
  pa_signal_new(SIGINT, exit_signal_callback, NULL);
  pa_signal_new(SIGTERM, exit_signal_callback, NULL);

  if (!(stdio_event = mainloop_api->io_new(mainloop_api,
                                              mode == PLAYBACK ? STDIN_FILENO : STDOUT_FILENO,
                                              mode == PLAYBACK ? PA_IO_EVENT_INPUT : PA_IO_EVENT_OUTPUT,
                                              mode == PLAYBACK ? stdin_callback : stdout_callback, NULL))) {
      fprintf(stderr, ("io_new() failed.\n"));
      goto quit;
  }

  /* Create a new connection context */
  if (!(context = pa_context_new(mainloop_api,(char*) client_name.get()))) {
      fprintf(stderr, ("pa_context_new() failed.\n"));
      goto quit;
  }

  pa_context_set_state_callback(context, context_state_callback, NULL);

  /* Connect the context */
  if (pa_context_connect(context, server, static_cast<pa_context_flags_t>(0) , NULL) < 0) {
      fprintf(stderr, ("pa_context_connect() failed: %s\n"), pa_strerror(pa_context_errno(context)));
      goto quit;
  }
  
  printf("mainloop...\n");

  /* Run the main loop */
  if (pa_mainloop_run(m, &ret) < 0) {
      fprintf(stderr, ("pa_mainloop_run() failed.\n"));
      goto quit;
  }

quit:
  if (stream)
      pa_stream_unref(stream);

  if (buffer) pa_xfree(buffer);

  if (stdio_event) {
      assert(mainloop_api);
      mainloop_api->io_free(stdio_event);
  }
  
  if(context) pa_context_unref(context);
  
  if (m) {
    pa_signal_done();
    pa_mainloop_free(m);
  }

  return 0;
}
