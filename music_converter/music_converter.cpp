// Music converter & beat detector
// (C) Copyright 2015 Petri Häkkinen. All rights reserved.
// Converts an Ogg file into 7-bit RAW PCM + beat data stored in LSB bit

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <algorithm>
#include "stb_vorbis.c"

#define MAKE_ID( a, b, c, d )	(((a))|((b)<<8)|((c)<<16)|(d<<24))

#define ID_RIFF					MAKE_ID('R', 'I', 'F', 'F')
#define ID_WAVE					MAKE_ID('W', 'A', 'V', 'E')
#define ID_FMT					MAKE_ID('f', 'm', 't', ' ')
#define ID_DATA					MAKE_ID('d', 'a', 't', 'a')

float* loadRAW(const char* filename, int* samples)
{
	// assuming fp32 raw format

	// load file
	FILE* fp = fopen(filename, "rb");
	if(!fp)
	{
		printf("Could not open file for reading: %s\n", filename);
		return 0;
	}
	fseek(fp, 0, SEEK_END);
	size_t size = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	float* data = (float*)malloc(size);
	fread(data, 1, size, fp);
	fclose(fp);

	*samples = size / 4;
	return data;
}

float* loadOGG(const char* filename, int* samples, int* sample_rate)
{
	// load and decode ogg file
	int channels = 0;
	short* decompressed = 0;
	*samples = stb_vorbis_decode_filename(filename, &channels, sample_rate, &decompressed);
	printf("Channels:    %d\n", channels);
	printf("Sample rate: %d\n", *sample_rate);
	printf("Samples:     %d\n", *samples);

	// convert to 32-bit floating point mono
	float* data = (float*)malloc(*samples * 4);
	for(int i = 0; i < *samples; i++)
		data[i] = decompressed[i*2] / 32768.0f;

	return data;
}

void writeRAW(const char* filename, const float* data_, int samples)
{
	// convert to 8-bit unsigned PCM
	uint8_t* data = (uint8_t*)malloc(samples);
	for(int i = 0; i < samples; i++)
		data[i] = std::min(std::max((int)(data_[i] * 127.0f + 128.0f), 0), 255);

	FILE* fp = fopen(filename, "wb");
	if(!fp)
	{
		printf("Could not open file for writing: %s\n", filename);
	}
	fwrite(data, 1, samples, fp);
	fclose(fp);

}

void writeWAV(const char* filename, const float* data, int samples, int sample_rate)
{
	struct WavHeader
	{
		uint32_t	riff;
		uint32_t	file_size;
		uint32_t	file_type;
	};

	struct WavFormat
	{
		uint32_t	chunk_id;
		uint32_t	chunk_size;
		uint16_t	format_tag;
		uint16_t	channels;
		uint32_t	sample_rate;
		uint32_t	bytes_per_sec;
		uint16_t	block_align;		// what is this?
		uint16_t	bits_per_sample;
	};

	FILE* fp = fopen(filename, "wb");
	if(!fp)
	{
		printf("Could not open file for writing: %s\n", filename);
		return;
	}

	// write header
	WavHeader header;
	header.riff = ID_RIFF;
	header.file_size = 4 + sizeof(WavFormat) + samples * 4 + 8;
	header.file_type = ID_WAVE;
	fwrite(&header, 1, sizeof(header), fp);

	// write format chunk
	WavFormat format;
	format.chunk_id = ID_FMT;
	format.chunk_size = sizeof(WavFormat) - 8;
	format.format_tag = 3; // 3 = IEEE float
	format.channels = 1;
	format.sample_rate = sample_rate;
	format.bytes_per_sec = sample_rate * 4;
	format.block_align = 4;
	format.bits_per_sample = 32;
	fwrite(&format, 1, sizeof(format), fp);

	// write data chunk
	uint32_t chunk_id = ID_DATA;
	uint32_t chunk_size = samples * 4;
	fwrite(&chunk_id, 1, 4, fp);
	fwrite(&chunk_size, 1, 4, fp);
	fwrite(data, 1, samples * 4, fp);

	fclose(fp);
}

float* resample(const float* data, int samples, float input_frequency, float output_frequency, int* out_samples)
{
	int new_samples = (int)(samples * output_frequency / input_frequency + 0.5f);
	float* new_data = (float*)malloc(new_samples * 4);

	for(int i = 0; i < new_samples; i++)
	{
		float t = i * (input_frequency / output_frequency);
		float z0 = data[std::min((int)t, samples-1)];
		float z1 = data[std::min((int)t+1, samples-1)];
		t = t - (int)t;
		new_data[i] = z0 * (1.0f - t) + z1 * t;		
	}

	*out_samples = new_samples;
	return new_data;
}

/*
Low-pass filter based on SoLoud library

SoLoud audio engine
Copyright (c) 2013-2014 Jari Komppa

This software is provided 'as-is', without any express or implied
warranty. In no event will the authors be held liable for any damages
arising from the use of this software.

Permission is granted to anyone to use this software for any purpose,
including commercial applications, and to alter it and redistribute it
freely, subject to the following restrictions:

   1. The origin of this software must not be misrepresented; you must not
   claim that you wrote the original software. If you use this software
   in a product, an acknowledgment in the product documentation would be
   appreciated but is not required.

   2. Altered source versions must be plainly marked as such, and must not be
   misrepresented as being the original software.

   3. This notice may not be removed or altered from any source
   distribution.
*/
void lowPassFilterBQR(float* buffer, int samples, float frequency, float samplerate, float resonance)
{
	float omega = (float)((2.0f * M_PI * frequency) / samplerate);
	float sin_omega = (float)sin(omega);
	float cos_omega = (float)cos(omega);
	float alpha = sin_omega / (2.0f * resonance);
	float scalar = 1.0f / (1.0f + alpha);

	float a0 = 0.5f * (1.0f - cos_omega) * scalar;
	float a1 = (1.0f - cos_omega) * scalar;
	float a2 = a0;
	float b1 = -2.0f * cos_omega * scalar;
	float b2 = (1.0f - alpha) * scalar;

	float y1 = 0.0f;
	float y2 = 0.0f;
	float x1 = 0.0f;
	float x2 = 0.0f;

	int c = 0;
	for(int i = 0; i < samples/2; i++)
	{
		// generate outputs by filtering inputs.
		float x = buffer[c];
		y2 = (a0 * x) + (a1 * x1) + (a2 * x2) - (b1 * y1) - (b2 * y2);
		buffer[c] += (y2 - buffer[c]);
		c++;

		// permute filter operations to reduce data movement.
		// just substitute variables instead of doing x1=x, etc.
		x2 = buffer[c];
		y1 = (a0 * x2) + (a1 * x) + (a2 * x1) - (b1 * y2) - (b2 * y1);
		buffer[c] += (y1 - buffer[c]);
		c++;

		// only move a little data.
		x1 = x2;
		x2 = x;
	}
}

float* beatDetector(const float* data, int samples)
{
	int window_width = 500;
	float threshold = 6.0f;
	int decay_length = 20;
	int decay = 0;

	// compare amplitude to average amplitude inside a moving window
	float* new_data = (float*)malloc(samples * 4);
	for(int i = 0; i < samples; i++)
	{
		// compute average amplitude
		float avg = 0.0f;
		int x0 = std::max(i - window_width/2, 0);
		int x1 = std::min(i + window_width/2, samples-1);
		for(int j = x0; j < x1; j++)
			avg += data[j] * data[j];
		avg /= (float)(x1 - x0);

		// compare amplitude of sample to average amplitude
		if(data[i] * data[i] > avg * threshold)
			decay = decay_length;
		new_data[i] = (decay-- > 0 ? 1.0f : 0.0f);
	}

	return new_data;
}

/*
void lowPassFilterSimple()
{
	// init filter by reading one filter width of data
	int sum = 0;
	for(int i = 0; i < filter_width; i++)
		sum += (int)data[i] - 128;

	// low pass filter
	// float* dest = (float*)malloc(size * 4);
	// for(size_t i = 0; i < size; i++)
	// {
	// 	dest[i] = (float)sum / (float)filter_width / 128.0f;
	// 	sum -= (int)data[i] - 128;
	// 	if(i + filter_width < size)
	// 		sum += (int)data[i + filter_width] - 128;
	// 	else
	// 		filter_width--;
	// }

	// low pass filter
	// good filter size somewhere between 32-64
	float* dest = (float*)malloc(size * 4 / filter_width);
	for(size_t i = 0; i < size/filter_width; i++)
	{
		int sum = 0;
		for(int j = 0; j < filter_width; j++)
			sum += data[i * filter_width + j] - 128;
		dest[i] = (float)sum / (float)filter_width / 128.0f;

		// 
		dest[i] = dest[i] * dest[i] * dest[i] * 2.0f;
	}
}
*/

int main(int argc, const char** argv)
{
	if(argc != 3)
	{
		printf("Usage: %s [input filename] [output filename]\n", argv[0]);
		return 0;
	}

	const char* input_filename = argv[1];
	const char* output_filename = argv[2];
	//float lowpass_frequency = atof(argv[3]);
	//float lowpass_resonance = atof(argv[4]);

	// load input file
	int samples = 0;
	int sample_rate = 0;
	float* data = loadOGG(input_filename, &samples, &sample_rate);

#if 0
	// run low-pass filter
	lowPassFilterBQR(data, samples, lowpass_frequency, sample_rate, lowpass_resonance);

	// resample to 500 Hz
	int new_samples = 0;
	data = resample(data, samples, sample_rate, 500.0f, &new_samples);
	samples = new_samples;
	sample_rate = 500.0f;

	// detect beat
	data = beatDetector(data, samples);
#endif

	// resample to 40000 Hz
	int new_samples = 0;
	data = resample(data, samples, sample_rate, 40000.0f, &new_samples);
	samples = new_samples;
	sample_rate = 40000.0f;

	// save result
	writeRAW(output_filename, data, samples);
	//writeWAV(output_filename, data, samples, sample_rate);

	return 0;
}
