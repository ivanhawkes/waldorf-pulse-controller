#include "MidiHandler.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <jack/midiport.h>
#include<QDebug>

jack_port_t *inputPort;
jack_port_t *outputPort;
jack_client_t *client;
jack_midi_data_t controlValue{0};
jack_midi_data_t controlValueChange{1};
bool shouldExitNow{false};


int OnJackProcess(jack_nframes_t noOfFrames, void *arg)
{
	qInfo() << "Jack: process";

	// You must get the buffer each cycle.
	void *inputBuffer = jack_port_get_buffer(inputPort, noOfFrames);

	// You must get the buffer each cycle.
	void *outputBuffer = jack_port_get_buffer(outputPort, noOfFrames);

	// Clear the output each process cycle.
	jack_midi_clear_buffer(outputBuffer);

	jack_midi_data_t *writeBuffer1 = jack_midi_event_reserve(outputBuffer, 0, 3);
	writeBuffer1[0] = 0xB0; // CC
	writeBuffer1[1] = 0x32; // 50
	writeBuffer1[2] = controlValue;

	jack_midi_data_t *writeBuffer2 = jack_midi_event_reserve(outputBuffer, 1, 3);
	writeBuffer2[0] = 0xB0; // CC
	writeBuffer2[1] = 0x34; // 52
	writeBuffer2[2] = controlValue;

	jack_midi_data_t *writeBuffer3 = jack_midi_event_reserve(outputBuffer, 2, 3);
	writeBuffer3[0] = 0xB0; // CC
	writeBuffer3[1] = 0x37; // 55
	writeBuffer3[2] = controlValue;

	jack_midi_data_t *writeBuffer4 = jack_midi_event_reserve(outputBuffer, 3, 3);
	writeBuffer4[0] = 0xB0; // CC
	writeBuffer4[1] = 0x38; // 56
	writeBuffer4[2] = controlValue;

	// Get number of events, and process them.
	jack_nframes_t inputEventCount = jack_midi_get_event_count(inputBuffer);
	if (inputEventCount > 0)
	{
		// Rapidly cycle up and down.
		// TODO: Use a timer to slow this down.
		controlValue += controlValueChange;
		if (controlValue >= 127)
		{
			controlValueChange = -1;
		}
		if (controlValue <= 0)
		{
			controlValueChange = 1;
		}

		for (jack_nframes_t i = 0; i < inputEventCount; i++)
		{
			jack_midi_event_t event;

			int success = jack_midi_event_get(&event, inputBuffer, i);
			if (success == 0)
			{
				// Ignore 0xF8 - seems to be a keep-alive from the Triton.
				if (event.buffer[0] != 0xF8)
				{
					if (event.size == 1)
					{
						printf("Midi Event: BYTES %lu [%0X]\n", event.size, event.buffer[0]);
					}
					else if (event.size == 2)
					{
						printf("Midi Event: BYTES %lu [%0X][%0X]\n", event.size, event.buffer[0], event.buffer[1]);
					}
					else if (event.size == 3)
					{
						// printf("Midi Event: BYTES %lu [%0X][%0X][%0X]\n", event.size, event.buffer[0],
						// event.buffer[1], event.buffer[2]);
					}
					else
					{
						printf(
							"Midi Event: BYTES %lu [%0X][%0X][%0X]...\n", event.size, event.buffer[0],
							event.buffer[1], event.buffer[2]);
					}

					// Let's examine the first byte to determine the message type.
					jack_midi_data_t firstByte = event.buffer[0];
					jack_midi_data_t channel = firstByte & 0x0F;
					jack_midi_data_t messageType = (firstByte >> 4) & 0x07;

					switch (messageType)
					{
						case 0:
							printf(
								"Time: [%0X]    Msg: [%0X]    Note Off:        Chan: [%0X]    Data: [%0X][%0X]\n",
								noOfFrames, event.buffer[0], channel, event.buffer[1], event.buffer[2]);
							break;

						case 1:
							printf(
								"Time: [%0X]    Msg: [%0X]    Note On:         Chan: [%0X]    Data: [%0X][%0X]\n",
								noOfFrames, event.buffer[0], channel, event.buffer[1], event.buffer[2]);
							break;

						case 2:
							printf(
								"Time: [%0X]    Msg: [%0X]    P. Aftertouch:   Chan: [%0X]    Data: [%0X][%0X]\n",
								noOfFrames, event.buffer[0], channel, event.buffer[1], event.buffer[2]);
							break;

						case 3:
							printf(
								"Time: [%0X]    Msg: [%0X]    Control Change:  Chan: [%0X]    Data: [%0X][%0X]\n",
								noOfFrames, event.buffer[0], channel, event.buffer[1], event.buffer[2]);
							break;

						case 4:
							printf(
								"Time: [%0X]    Msg: [%0X]    Program Change:  Chan: [%0X]    Data: [%0X][%0X]\n",
								noOfFrames, event.buffer[0], channel, event.buffer[1], event.buffer[2]);
							break;

						case 5:
							printf(
								"Time: [%0X]    Msg: [%0X]    C. Aftertouch:   Chan: [%0X]    Data: [%0X][%0X]\n",
								noOfFrames, event.buffer[0], channel, event.buffer[1], event.buffer[2]);
							break;

						case 6:
							printf(
								"Time: [%0X]    Msg: [%0X]    Pitch Wheel:     Chan: [%0X]    Data: [%0X][%0X]\n",
								noOfFrames, event.buffer[0], channel, event.buffer[1], event.buffer[2]);
							break;

						default:
							printf(
								"Time: [%0X]    Msg: [%0X]    Unknown:         Chan: [%0X]    Data: [%0X][%0X]\n",
								noOfFrames, event.buffer[0], channel, event.buffer[1], event.buffer[2]);
							break;
					}
				}
			}
			else
			{
				printf("Midi Event Failure! %i\n", success);
			}
		}
	}

	return 0;
}


void OnJackShutdown(void *arg)
{
	shouldExitNow = true;
}


int JackInit()
{
	jack_status_t status;
	jack_options_t options{JackNullOption};

	qInfo() << "Jack Test Started Init ";

	client = jack_client_open("JackTest", options, &status);
	if (client == nullptr)
	{
		qCritical() << "jack_client_open() failed. status = " << status;
		if (status & JackServerFailed)
		{
			qCritical() << "unable to connect to JACK server.\n";
		}

		shouldExitNow = true;
		return 1;
	}

	if (status)
	{
		if (JackServerStarted)
			qInfo() << "JACK server started.\n";

		if (JackNameNotUnique)
			qInfo() << "unique name " << jack_get_client_name(client) << " assigned";
	}

	// Register a device input MIDI port.
	inputPort = jack_port_register(client, "DeviceInput", JACK_DEFAULT_MIDI_TYPE, JackPortIsInput, 0);
	if (inputPort == NULL)
	{
		qCritical() << "no more JACK input port available.\n";
		shouldExitNow = true;
		return 1;
	}

	// Register a device output MIDI port.
	outputPort = jack_port_register(client, "DeviceOutput", JACK_DEFAULT_MIDI_TYPE, JackPortIsOutput, 0);
	if (outputPort == NULL)
	{
		qCritical() << "no more JACK output port available.\n";
		shouldExitNow = true;
		return 1;
	}

	// Set the callback for the main process.
	jack_set_process_callback(client, OnJackProcess, 0);

	// Set the callback for the shutdown process.
	jack_on_shutdown(client, OnJackShutdown, 0);

	// Tell the server we're ready to process.
	if (jack_activate(client))
	{
		qCritical() << "cannot activate client";
		shouldExitNow = true;
		return 1;
	}

	const char **inPorts = jack_get_ports(client, NULL, JACK_DEFAULT_MIDI_TYPE, 0);

	if (inPorts)
	{
		int i{0};
		printf("In Port List:\n");
		while (inPorts[i] != nullptr)
		{
			qInfo() << "Port Name: " << inPorts[i] << "     ";

			jack_port_t *port = jack_port_by_name(client, inPorts[i]);
			if (port)
			{
				int flags = jack_port_flags(port);
				qInfo() << "Flags: " << flags;
			}

			i++;
		}
		jack_free(inPorts);
	}

	qInfo() << "Connection 1: " << jack_connect(client, "alsa_midi:MidiSport 4x4 MIDI 1 (out)", "JackTest:DeviceInput");
	qInfo() << "Connection 2: " << jack_connect(client, "JackTest:DeviceOutput", "alsa_midi:MidiSport 4x4 MIDI 3 (in)");
	qInfo() << "Connection 3: " << jack_connect(client, "alsa_midi:MidiSport 4x4 MIDI 1 (out)", "alsa_midi:MidiSport 4x4 MIDI 3 (in)");

	qInfo() << "Init ran to completion: ";

	return 0;
}


void JackExit()
{
	qInfo() << "Jack: exiting";

	// We're finished using Jack. Tell it to cleanup and close down.
	if (client)
		jack_client_close(client);
}
