#!/bin/bash

if [ "${XDEV68K_DIR}" == "" ]; then
  echo "error: XDEV68K_DIR environment variable is not defined."
  exit 1
fi

WORKING_DIR=`pwd`

CC=${XDEV68K_DIR}/m68k-toolchain/bin/m68k-elf-gcc
GAS2HAS="${XDEV68K_DIR}/util/x68k_gas2has.pl -cpu 68000 -inc doscall.inc -inc iocscall.inc"
RUN68=${XDEV68K_DIR}/run68/run68
HAS=${XDEV68K_DIR}/x68k_bin/HAS060.X
#HLK=${XDEV68K_DIR}/x68k_bin/hlk301.x
HLK=${XDEV68K_DIR}/x68k_bin/LK.X
HLK_LINK_LIST=_lk_list.tmp

INCLUDE_FLAGS="-I${XDEV68K_DIR}/include/xc -I${XDEV68K_DIR}/include/xdev68k -I${WORKING_DIR}"
COMMON_FLAGS="-m68000 -Os ${INCLUDE_FLAGS} -z-stack=32768"
CFLAGS="${COMMON_FLAGS} -Wno-builtin-declaration-mismatch -fcall-used-d2 -fcall-used-a2 \
    -fexec-charset=cp932 -fverbose-asm -fno-defer-pop -DFPM_DEFAULT -D_TIME_T_DECLARED -D_CLOCK_T_DECLARED -Dwint_t=int \
		-DXDEV68K -DHAVE_CONFIG_H -DHAVE_AUBIO_DOUBLE=1 -DHAVE_S44READ -DHAVE_WAVREAD"

function do_compile() {
  pushd .
  cd $1
  rm -rf _build
  mkdir -p _build
  for c in $2; do
    echo "compiling ${c}.c in ${1} ..."
	  ${CC} -S ${CFLAGS} -o _build/${c}.m68k-gas.s ${c}.c
    if [ ! -f _build/${c}.m68k-gas.s ]; then
      return 1
    fi
	  perl ${GAS2HAS} -i _build/${c}.m68k-gas.s -o _build/${c}.s
	  rm -f _build/${c}.m68k-gas.s
	  ${XDEV68K_DIR}/run68/run68 ${HAS} -e -u -w0 ${INCLUDE_FLAGS} _build/${c}.s -o _build/${c}.o
    if [ ! -f _build/${c}.o ]; then
      return 1
    fi
  done
  popd
  return 0
}

function build_aubio() {
  do_compile .          "cvec fmat fvec lvec mathutils musicutils vecutils"
  do_compile effects    "pitchshift_dummy timestretch_dummy"
  do_compile io         "audio_unit ioutils source_s44read source_wavread source"
  do_compile notes      "notes"
  do_compile onset      "onset peakpicker"
  do_compile pitch      "pitch pitchfcomb pitchmcomb pitchschmitt pitchspecacf pitchyin pitchyinfast pitchyinfft"
  do_compile spectral   "awhitening dct_accelerate dct_fftw dct_ipp dct_ooura dct_plain dct fft filterbank_mel filterbank mfcc ooura_fft8g phasevoc specdesc statistics tss"
  do_compile synth      "sampler wavetable"
  do_compile tempo      "beattracking tempo"
  do_compile temporal   "a_weighting biquad c_weighting filter resampler"
  do_compile utils      "hist log parameter scale strutils"
}

build_aubio
