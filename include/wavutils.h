#ifndef WAVLOADING_H
#define WAVLOADING_H

#include <iostream>
#include <cstdio>
#include <string>

#include <al.h>
#include <alc.h>

namespace wu
{

typedef unsigned int DWORD;
typedef unsigned char BYTE;

struct WavInfo
{
    BYTE *data;
    ALsizei data_size;
    ALuint frequency;
    ALenum format;
};

bool endWithError(std::string msg, bool error=false){std::cout << msg << "\n";return error;}

/*
 * Load wave file function. No need for ALUT with this
 * source: https://forums.gentoo.org/viewtopic-t-939234-view-next.html?sid=5e0089b50c410388124f41e7ef6ef36e
 */
bool loadWavFile(const std::string &filename, WavInfo &infos)
{
    FILE *fp = NULL;
    fp = fopen(filename.c_str(),"rb");
    if(!fp)
        return endWithError("WAUTILS::LOADWAVFILE::ERROR Failed to create file");

    char type[4];
    DWORD size,chunkSize;
    short formatType,channels;
    DWORD sampleRate,avgBytesPerSec;
    short bytesPerSample,bitsPerSample;
    DWORD dataSize;

    fread(type,sizeof(char),4,fp);
    if(type[0]!='R' || type[1]!='I' || type[2]!='F' || type[3]!='F')
        return endWithError("WAUTILS::LOADWAVFILE::ERROR While loading file: 'RIFF' missing");

    fread(&size, sizeof(DWORD),1,fp);
    fread(type, sizeof(char),4,fp);
    if (type[0]!='W' || type[1]!='A' || type[2]!='V' || type[3]!='E')
        return endWithError("WAUTILS::LOADWAVFILE::ERROR While loading file: 'WAVE' missing");

    fread(type,sizeof(char),4,fp);
    if (type[0]!='f' || type[1]!='m' || type[2]!='t' || type[3]!=' ')
     return endWithError("WAUTILS::LOADWAVFILE::ERROR While loading file: 'fmt ' missing");

    fread(&chunkSize,sizeof(DWORD),1,fp);
    fread(&formatType,sizeof(short),1,fp);
    fread(&channels,sizeof(short),1,fp);
    fread(&sampleRate,sizeof(DWORD),1,fp);
    fread(&avgBytesPerSec,sizeof(DWORD),1,fp);
    fread(&bytesPerSample,sizeof(short),1,fp);
    fread(&bitsPerSample,sizeof(short),1,fp);

    fread(type,sizeof(char),4,fp);

    if (type[0]!='d' || type[1]!='a' || type[2]!='t' || type[3]!='a')
        return endWithError("WAUTILS::LOADWAVFILE::ERROR While loading file: 'data' missing");

    fread(&dataSize,sizeof(DWORD),1,fp);

    std::cout << "Chunk Size: " << chunkSize << "\n";
    std::cout << "Format Type: " << formatType << "\n";
    std::cout << "Channels: " << channels << "\n";
    std::cout << "Sample Rate: " << sampleRate << "\n";
    std::cout << "Average Bytes Per Second: " << avgBytesPerSec << "\n";
    std::cout << "Bytes Per Sample: " << bytesPerSample << "\n";
    std::cout << "Bits Per Sample: " << bitsPerSample << "\n";
    std::cout << "Data Size: " << dataSize << "\n";

    unsigned char* buf= new unsigned char[dataSize];
    std::cout << fread(buf,sizeof(BYTE),dataSize,fp) << " bytes loaded\n";

    infos.frequency = sampleRate;
    infos.data_size = dataSize;
    infos.data = buf;

    infos.format = 0;
    if(bitsPerSample == 8)
    {
        if(channels == 1)
            infos.format = AL_FORMAT_MONO8;
        else if(channels == 2)
            infos.format = AL_FORMAT_STEREO8;
    }
    else if(bitsPerSample == 16)
    {
        if(channels == 1)
            infos.format = AL_FORMAT_MONO16;
        else if(channels == 2)
            infos.format = AL_FORMAT_STEREO16;
    }
    if(!infos.format)
        return endWithError("WAUTILS::LOADWAVFILE::ERROR While loading file: wrong bits per sample");

    fclose(fp);

    return true;
}

/*
 * Save wav file from data
 * */
bool saveWavFile(const char* filename, const WavInfo &infos)
{
    //  Opening file
    FILE *fp = NULL;
    fp = fopen(filename,"wb");
    if(!fp)
        return endWithError("WAUTILS::SAVEWAVFILE::ERROR Failed to create file");

    DWORD   file_size = 4 * sizeof(char) + 5 * sizeof(DWORD) + 4 * sizeof(short) + (infos.data_size-8) * sizeof(BYTE),
            chunk_size = 0x10,
            avg_bytes_per_sec;
    short   format_type = 1,
            channels,
            bytes_per_sample,
            bits_per_sample;

    //  WAVE file bloc
    char riff[] = "RIFF";
    fwrite(riff, sizeof(char), 4, fp);
    fwrite(&file_size, sizeof(DWORD),1,fp);
    char wave[] = "WAVE";
    fwrite(wave, sizeof(char), 4, fp);


    if(infos.format == AL_FORMAT_MONO8)
    {
        bits_per_sample = 8;
        channels = 1;
    }
    else if(infos.format == AL_FORMAT_STEREO8)
    {
        bits_per_sample = 8;
        channels = 2;
    }
    else if(infos.format == AL_FORMAT_MONO16)
    {
        bits_per_sample = 16;
        channels = 1;
    }
    else if(infos.format == AL_FORMAT_STEREO16)
    {
        bits_per_sample = 16;
        channels = 2;
    }
    bytes_per_sample = channels * bits_per_sample / 8;
    avg_bytes_per_sec = infos.frequency * bytes_per_sample;


    //  Format bloc
    char fmt[] = "fmt ";
    fwrite(fmt, sizeof(char), 4, fp);
    fwrite(&chunk_size,sizeof(DWORD),1,fp);
    fwrite(&format_type,sizeof(short),1,fp);
    fwrite(&channels,sizeof(short),1,fp);
    fwrite(&infos.frequency,sizeof(DWORD),1,fp);
    fwrite(&avg_bytes_per_sec,sizeof(DWORD),1,fp);
    fwrite(&bytes_per_sample,sizeof(short),1,fp);
    fwrite(&bits_per_sample,sizeof(short),1,fp);

    //  Data bloc
    char data_[] = "data";
    fwrite(data_,sizeof(char),4,fp);
    fwrite(&infos.data_size,sizeof(DWORD),1,fp);
    fwrite(infos.data,sizeof(BYTE),infos.data_size,fp);

    fclose(fp);

    return true;
}

}

#endif // WAVLOADING_H
