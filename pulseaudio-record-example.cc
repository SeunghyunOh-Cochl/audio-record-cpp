#include <pulse/error.h>
#include <pulse/gccmacro.h>
#include <pulse/simple.h>
#include <signal.h>
#include <chrono>
#include <iostream>
#define SAMPLE_RATE 22050
#define BUF_SIZE (SAMPLE_RATE) / 2

// g++ pulseaudio-record-example.cc -o pulseaudio-record-example -lm -std=c++11 -ldl -lstdc++ -lpulse -lpulse-simple

void finish(pa_simple *s) {
  if (s) pa_simple_free(s);
}

static bool running = true;

/* Signals handling */
static void handle_sigterm(int signo) { running = false; }

void init_signal() {
  struct sigaction sa;
  sa.sa_flags = 0;

  sigemptyset(&sa.sa_mask);
  sa.sa_handler = handle_sigterm;
  sigaction(SIGTERM, &sa, NULL);
  sigaction(SIGINT, &sa, NULL);
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
  static pa_sample_spec ss;
  ss.format = PA_SAMPLE_S16LE;  // May vary based on your system
  ss.rate = SAMPLE_RATE;
  ss.channels = 1;

  init_signal();

  pa_simple *s = NULL;
  
  static pa_buffer_attr buf_attr;
  // buf_attr.maxlength = (uint32_t) 1073741824; // 1G
  // buf_attr.maxlength = (uint32_t) 8448; // 0.1%
  buf_attr.maxlength = (uint32_t) 2000; // 0.0...%
  buf_attr.fragsize = (uint32_t) 0;
  buf_attr.minreq = 0;
  buf_attr.prebuf = (uint32_t) -1;
  buf_attr.tlength = 0;
  
  int error;
  // Create the recording stream
  if (!(s = pa_simple_new(NULL, argv[0], PA_STREAM_RECORD, NULL, "record", &ss,
                          NULL, &buf_attr, &error))) {
    fprintf(stderr, __FILE__ ": pa_simple_new() failed: %s\n",
            pa_strerror(error));
    finish(s);
    return -1;
  }

  int16_t* buffer = (int16_t*) malloc(BUF_SIZE*sizeof(int16_t));
  while (running) {   
    auto start = std::chrono::high_resolution_clock::now(); 
    /* Record some data ... */
    if (pa_simple_read(s, (int16_t*) buffer, BUF_SIZE*sizeof(int16_t), &error) < 0) {
      fprintf(stderr, __FILE__ ": pa_simple_read() failed: %s\n",
              pa_strerror(error));
      finish(s);
      return -1;
    }
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    fprintf(stdout, "read %d done %d ms \n", buffer[0], duration);
  }

  free(buffer);
  finish(s);
  return 0;
}
