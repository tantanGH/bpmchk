#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <iocslib.h>
#include <doslib.h>
#include <aubio.h>

#define VERSION "0.1.0 (2023/03/02)"

#define AUBIO_WIN_SIZE (1024)      // FFT size
#define AUBIO_HOP_SIZE (512)       // HOP size

#define FORMAT_RAW  (0)
#define FORMAT_WAVE (1)

#define MAX_BEAT_TIMES (1024)

// quick sort comparator
static int compare_float(const void* key1, const void* key2) {
  smpl_t f1 = *((const float*)key1);
  smpl_t f2 = *((const float*)key2);
  return f1 < f2 ? -1 : f1 > f2 ? 1 : 0;
}

// bpm estimation
static float estimate_bpm(smpl_t* beat_times, size_t beat_count) {

  static float bpms[ MAX_BEAT_TIMES ];

  for (int16_t i = 0; i < beat_count - 1; i++) {
    bpms[i] = 60.0 / ( beat_times[ i + 1 ] - beat_times[ i ] );
  }

  qsort(bpms, beat_count - 1, sizeof(float), compare_float);

  return bpms[ ( beat_count - 1 ) / 2 ];      // median
}

// main
int32_t main(int32_t argc, uint8_t* argv[]) {

  int32_t rc = -1;

  printf("BPMCHK.X - PCM data BPM checker version " VERSION " by tantan\n");

  if (argc < 2) {
    printf("usage: bpmchk <pcm-file[.s(32|44|48)|.m(32|44|48)|.wav]\n");
    return rc;
  }

  uint8_t* pcm_file_name = argv[1];

  FILE* fp = fopen(pcm_file_name, "rb");
  if (fp == NULL) {
    printf("error: pcm file open error.\n");
    goto exit;
  }
  fseek(fp, 0, SEEK_END);
  size_t pcm_file_size = ftell(fp);
  fclose(fp);

  int32_t pcm_freq = 0;
  int16_t pcm_channels = 0;
  int16_t pcm_format = FORMAT_RAW;
  
  if (stricmp(pcm_file_name + strlen(pcm_file_name) - 4, ".wav") == 0) {
    pcm_freq = 0;
    pcm_channels = 0;
    pcm_format = FORMAT_WAVE;
  }

  aubio_source_t* aubio_source = new_aubio_source(pcm_file_name, 0, AUBIO_HOP_SIZE);

  if (pcm_freq == 0) {
    pcm_freq = aubio_source_get_samplerate(aubio_source);
  }
  if (pcm_channels == 0) {
    pcm_channels = aubio_source_get_channels(aubio_source);
  }

  // describe source file information
  printf("\n");
  printf("File name     : %s\n", pcm_file_name);
  printf("Data size     : %d [bytes]\n", pcm_file_size);
  printf("Data format   : %s\n", 
            pcm_format == FORMAT_WAVE ? "WAVE" : 
            "16bit signed raw PCM (big endian)");

  printf("PCM frequency : %d [Hz]\n", pcm_freq);
  printf("PCM channels  : %s\n", pcm_channels == 1 ? "mono" : "stereo");

  uint32_t pcm_duration = aubio_source_get_duration(aubio_source);
  if (pcm_duration > 0) {
    printf("PCM length    : %4.2f [sec]\n", (float)pcm_duration / (float)pcm_freq);
    printf("PCM duration  : %d [frames]\n", pcm_duration);
  }

  fvec_t* aubio_buffer = new_fvec( AUBIO_HOP_SIZE );
  uint_t n_frames = 0;
  uint_t frames_read = 0;
  uint_t last_beat_pos = 0;
  uint_t measure_count = 0;

  aubio_tempo_t * o = new_aubio_tempo("default", AUBIO_WIN_SIZE, AUBIO_HOP_SIZE, pcm_freq);
  fvec_t* tempo_out = new_fvec(2);
  float delay = 4.0f * AUBIO_HOP_SIZE;

  static smpl_t beat_times[ MAX_BEAT_TIMES ];
  size_t beat_ofs = 0;

  uint_t abort = 0;
  uint_t t0 = ONTIME();
  printf("\n");

  do {

    // put some fresh data in input vector
    aubio_source_do(aubio_source, aubio_buffer, &frames_read);

    // execute tempo
    aubio_tempo_do(o, aubio_buffer, tempo_out);

    // do something with the beats
    if (tempo_out->data[0] != 0) {
      beat_times[ beat_ofs++ ] = aubio_tempo_get_last_s(o);
      if ((beat_ofs % 8) == 0) {
        printf("\nEstimated BPM = %4.2f\n", estimate_bpm(beat_times, beat_ofs));
      }
      if (beat_ofs >= MAX_BEAT_TIMES) break;
    }
    n_frames += frames_read;

    printf("\rAnalyzed %d/%d frames (%4.2f%%) in %4.2f sec ([SHIFT] key to quit)", 
          n_frames, pcm_duration, n_frames * 100.0 / pcm_duration, (ONTIME() - t0)/100.0);

    if (B_SFTSNS() & 0x01) {
      printf("\n");
      abort = 1;
      break;
    }

  } while ( frames_read == AUBIO_HOP_SIZE );

  if (!abort) {
    printf("\n\nFinished.\n");
    printf("Estimated BPM = %4.2f\n", estimate_bpm(beat_times, beat_ofs));
    rc = 0;
  }

catch:
  if (tempo_out != NULL) {
    del_fvec(tempo_out);
    tempo_out = NULL;
  }
  if (o != NULL) {
    del_aubio_tempo(o);
    o = NULL;
  }
  if (aubio_buffer != NULL) {
    del_fvec(aubio_buffer);
    aubio_buffer = NULL;
  }
  if (aubio_source != NULL) {
    del_aubio_source(aubio_source);
    aubio_source = NULL;
  }

exit:
  return rc;
}
