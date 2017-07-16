#include <iostream>
#include <cstdio>
#include <string>
#include <math.h>
#include <numeric>
#include <vector>
#include <al.h>
#include <alc.h>
#include <opencv2/opencv.hpp>

#include "wavutils.h"

//  Constants
#define NB_MAX_SOURCES 3
#define MIN_CONTOUR_AREA 200
#define MIN_DISTANCE_BETWEEN_CONTOURS 200

#define DEBUG   //  Comment this line not to display rectangles
                //  Blue  -> Largest candidate bounding box
                //  Green -> Others bounding boxes
                //  Red   -> Final bounding box

using namespace std;
using namespace cv;

//  Functions prototypes
bool launchProcess(const std::string &video_file, const std::string &audio_file);
bool initAudio(const std::string &audio_filename, ALuint *&sources, ALCdevice *&emit_device, ALCcontext *&context, ALuint &emission_buffer, wu::WavInfo &emission_infos);
void cleanUpAudio(ALuint *&sources, ALCdevice *&emit_device, ALCcontext *&context, ALuint &emission_buffer, wu::WavInfo &emission_infos);
Mat frameProcessing(const Mat &frame);
Mat backgroundSubstraction(const Mat &background, const Mat &frame);
std::vector<Rect> processContours(const Mat &frame, Mat &frame_disp);


/*
 * MAIN FUNCTION
 * */
int main(int argc, char *argv[])
{
    std::string audio_file1("../car.wav"),
                video_file1("../video.m4v");

    launchProcess(video_file1, audio_file1);
    //launchProcess(video_file1, audio_file1);

    return EXIT_SUCCESS;
}

bool launchProcess(const std::string &video_file, const std::string &audio_file)
{
    ALuint      *sources;
    ALCdevice   *emit_device;
    ALCcontext  *context;
    ALuint      emission_buffer;
    wu::WavInfo emission_infos;
    Mat         frame,
                first_frame,
                thresh;

    //  Set frame to display
    Mat *display_frame = &first_frame;

    if(!initAudio(audio_file, sources, emit_device, context, emission_buffer, emission_infos))
        return EXIT_FAILURE;
    VideoCapture cap(video_file);
    if(!cap.isOpened())
    {
        std::cerr << "VIDEOEDITOR::CUTVIDEO::ERROR Can't open video" << std::endl;
        return EXIT_FAILURE;
    }
    namedWindow("Sonorizer");


    //  Extract 1st frame (background)
    cap >> frame;
    first_frame = frameProcessing(frame);

    //  MAIN LOOP
    ALboolean loop_end = AL_FALSE;
    while(!loop_end)
    {
        cap >> frame;
        if(!frame.data) //  End
            break;

        thresh = backgroundSubstraction(first_frame, frameProcessing(frame));
        std::vector<Rect> regions_of_interest = processContours(thresh, frame);

        //  Video in realtime
        char k = (char)waitKey(300);
        if(k == 27)
            break;

        if(regions_of_interest.size() > 0) // Is there anything moving ?
        {
            for(unsigned int source_index = 0; source_index < NB_MAX_SOURCES; ++source_index)  //  Set sources pos according to roi pos
            {
                ALint source_state;
                if(source_index < regions_of_interest.size())    // If current source index is lower than number of roi
                {
                    ALfloat SourcePos[] = { 0.0, 0.0, 0.0 };

                    //  Set source position
                    SourcePos[0] = ((float)(regions_of_interest[source_index].x + regions_of_interest[source_index].width / 2) / frame.cols) * 2 - 1;
                    SourcePos[2] = -1.0f/((float)regions_of_interest[source_index].height / frame.rows * 5) * 10;   // Parameters tuning, it works!

                    alSourcefv(sources[source_index], AL_POSITION, SourcePos);

                    //  Play source (if not already playing)
                    alGetSourcei(sources[source_index], AL_SOURCE_STATE, &source_state);
                    if(source_state != AL_PLAYING)
                        alSourcePlay(sources[source_index]);

#ifdef DEBUG    //  Display final rectangle
                    rectangle(*display_frame, regions_of_interest[source_index], Scalar(0,0,255));
#endif
                }
                else    //  Else, pause the source
                {
                    alGetSourcei(sources[source_index], AL_SOURCE_STATE, &source_state);
                    if(source_state == AL_PLAYING)
                        alSourcePause(sources[source_index]);
                }
            }
        }
        else
        {
            for(unsigned int source_index = 0; source_index < NB_MAX_SOURCES; ++source_index)  //  Nothing is moving, pause every source
            {
                ALint source_state;
                alGetSourcei(sources[source_index], AL_SOURCE_STATE, &source_state);
                if(source_state == AL_PLAYING)
                    alSourcePause(sources[source_index]);
            }
        }
        imshow("Sonorizer", *display_frame);
    }

    cleanUpAudio(sources, emit_device, context, emission_buffer, emission_infos); //  End, clean up
}

/*
 * Init OpenAL stuff,
 * listener and sources
 * */
bool initAudio(const std::string &audio_filename, ALuint *&sources, ALCdevice *&emit_device, ALCcontext *&context, ALuint &emission_buffer, wu::WavInfo &emission_infos)
{
    emit_device = alcOpenDevice(NULL);
    if(!emit_device)
        wu::endWithError("AUDIOGENERATOR::GENERATEAUDIO::ERROR No sound device");

    context = alcCreateContext(emit_device, NULL);
    alcMakeContextCurrent(context);
    if(!context)
        wu::endWithError("AUDIOGENERATOR::GENERATEAUDIO::ERROR No sound context");


    //  Load input file
    if(!wu::loadWavFile(audio_filename, emission_infos))
        return wu::endWithError("AUDIOGENERATOR::GENERATEAUDIO::ERROR Can't load file");

    //  Emission buffer
    alGenBuffers(1, &emission_buffer);
    alBufferData(emission_buffer, emission_infos.format, emission_infos.data, emission_infos.data_size, emission_infos.frequency);
    if(alGetError() != AL_NO_ERROR)
        return wu::endWithError("AUDIOGENERATOR::GENERATEAUDIO::ERROR While loading ALBuffer");


    //  Init listener
    ALfloat ListenerPos[] = { 0.0, 0.0, 0.0 };
    ALfloat ListenerVel[] = { 0.0, 0.0, 0.0 };
    ALfloat ListenerOri[] = { 0.0, 0.0, -1.0,  0.0, 1.0, 0.0 };

    alListenerfv(AL_POSITION,    ListenerPos);
    alListenerfv(AL_VELOCITY,    ListenerVel);
    alListenerfv(AL_ORIENTATION, ListenerOri);


    //  Init sources with default values
    sources = new ALuint[NB_MAX_SOURCES];

    alGenSources(NB_MAX_SOURCES, sources);

    ALfloat SourcePos[] = { 1.0, 0.0, 0.0 };
    ALfloat SourceVel[] = { 0.0, 0.0, 0.0 };

    for(size_t i = 0; i < NB_MAX_SOURCES; ++i)
    {
        alSourcei (sources[i], AL_BUFFER,   emission_buffer);
        alSourcef (sources[i], AL_PITCH,    1.0f     );
        alSourcef (sources[i], AL_GAIN,     1.0f     );
        alSourcefv(sources[i], AL_POSITION, SourcePos);
        alSourcefv(sources[i], AL_VELOCITY, SourceVel);
        alSourcei (sources[i], AL_LOOPING,  AL_TRUE );
    }

    return true;
}

//  Clean OpenAL stuff
void cleanUpAudio(ALuint *&sources, ALCdevice *&emit_device, ALCcontext *&context, ALuint &emission_buffer, wu::WavInfo &emission_infos)
{
    delete[] emission_infos.data;
    alDeleteSources(NB_MAX_SOURCES, sources);
    alDeleteBuffers(1, &emission_buffer);
    alcMakeContextCurrent(NULL);
    alcDestroyContext(context);
    alcCloseDevice(emit_device);
}

// Comparison function (used for std::sort) to sort decreasing contours
bool compareContourAreas(std::vector<Point> contour1, std::vector<Point> contour2)
{
    double area1 = fabs(contourArea(cv::Mat(contour1)));
    double area2 = fabs(contourArea(cv::Mat(contour2)));
    return (area1 > area2);
}

/*
 * Process frame, will be used for 2 purposes:
 *  1-create background reference image
 *  2-transform next frames the same way, in order to compare them to reference
 * */
Mat frameProcessing(const Mat &frame)
{
    Mat out_frame,
        gray;

    cvtColor(frame, gray, COLOR_BGR2GRAY);          //  Convert to gray levels
    GaussianBlur(gray, out_frame, Size(21,21), 0);  //  Apply a blur, to reduce noise

    return out_frame;
}

/*
 * Substract background of a frame,
 * return the threshold of absolute diff
 * */
Mat backgroundSubstraction(const Mat &background, const Mat &frame)
{
    Mat diff,
        thresh;

    absdiff(background, frame, diff);
    threshold(diff, thresh, 35, 255, THRESH_BINARY);

    return thresh;
}

/*
 * Process contours of a frame to extract
 * regions of interest
 *
 * for every roi
 * if large enough
 * compare distance to others roi, if far enough, add roi
 * */
std::vector<Rect> processContours(const Mat &frame, Mat &frame_disp)
{
    std::vector<Rect> regions_of_interest;
    std::vector<std::vector<Point>> contours;
    std::vector<Vec4i> hierarchy;

    findContours(frame, contours, hierarchy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE); //  Extract contours

    if(contours.size() > 0)
    {
        std::sort(contours.begin(), contours.end(), compareContourAreas);   //  Decreasing sort of contours' areas

        //  1st, Largest contour
        std::vector<Point> c = contours[0];
        if(contourArea(c) > MIN_CONTOUR_AREA)    //  If large enough
        {
            Rect rect = boundingRect(c);
            regions_of_interest.push_back(rect);    //  Insert roi
#ifdef DEBUG
            rectangle(frame_disp, rect, Scalar(255,0,0));
#endif
            //  Process following contours
            size_t i = 1;
            while(i < contours.size() && contourArea(contours[i]) > MIN_CONTOUR_AREA)
            {
                c = contours[i];
                rect = boundingRect(c);

                bool is_next_to_roi = false;
                for(size_t j = 0; j < regions_of_interest.size(); ++j)  //  Compare distance from each roi
                {
                    //  if x distance between 2 roi is lower than the width of the largest of the 2 roi
                    if(abs((rect.x + rect.width / 2) - (regions_of_interest[j].x + regions_of_interest[j].width / 2)) < max(rect.width, regions_of_interest[j].width))
                    {
                        // current roi is above/under another, compute new y and height of the final roi
                        unsigned int y_max = max(regions_of_interest[j].y + regions_of_interest[j].height, rect.y + rect.height);
                        regions_of_interest[j].y = min(regions_of_interest[j].y, rect.y);
                        regions_of_interest[j].height = y_max - regions_of_interest[j].y;

                        is_next_to_roi = true;
                    }
                }
                if(!is_next_to_roi)
                    regions_of_interest.push_back(rect);    //  Insert roi
                ++i;
#ifdef DEBUG
                rectangle(frame_disp, rect, Scalar(0,255,0));
#endif
            }
        }

    }

    return regions_of_interest;
}
