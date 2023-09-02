#pragma once

#include <jack/jack.h>

extern bool shouldExitNow;

/* Called when Jack needs us to process frames of data. */
int OnJackProcess(jack_nframes_t noOfFrames, void *arg);

/* Called when Jack has shut down. We need to quickly quit work and exit. */
void OnJackShutdown(void *arg);

/* Must be called to initialise the Jack subsystem. This starts Jack processing. */
int JackInit();

/* Must be called to clean up after running. */
void JackExit();
