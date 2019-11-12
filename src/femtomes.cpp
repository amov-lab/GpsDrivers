/****************************************************************************
 *
 *   Copyright (c) 2012-2018 PX4 Development Team. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name PX4 nor the names of its contributors may be
 *    used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 ****************************************************************************/

#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <ctime>
#include <cmath>

#include "femtomes.h"
#include "rtcm.h"

#ifndef M_PI_F
# define M_PI_F 3.14159265358979323846f
#endif

#define MIN(X,Y)	((X) < (Y) ? (X) : (Y))
#define FEMTO_UNUSED(x) (void)x;

#if defined _FMTOMES_DEBUG
#define _FEMTOMES_DEBUG_LEVEL	2
#else
#define _FEMTOMES_DEBUG_LEVEL	2
#endif

#if _FEMTOMES_DEBUG_LEVEL
	#define FEMTO_INFO(...)			{GPS_INFO(__VA_ARGS__);}
	#define FEMTO_DEBUG(...)		{GPS_WARN(__VA_ARGS__);}
	#define FEMTO_ERR(...)			{GPS_ERR(__VA_ARGS__);}
#else
	#define FEMTO_INFO(...)			{(void)0;}
	#define FEMTO_DEBUG(...)		{(void)0;}
	#define FEMTO_ERR(...)			{(void)0;}
#endif


GPSDriverFemto::GPSDriverFemto(GPSCallbackPtr callback, void *callback_user,
				   struct vehicle_gps_position_s *gps_position,
				   struct satellite_info_s *satellite_info, float heading_offset) :
	GPSBaseStationSupport(callback, callback_user),
	_satellite_info(satellite_info),
	_gps_position(gps_position),
	_heading_offset(heading_offset)
{
	decodeInit();
}

GPSDriverFemto::~GPSDriverFemto()
{
	if (_rtcm_parsing) {
		delete (_rtcm_parsing);
	}
}

int GPSDriverFemto::handleMessage(int len)
{
    int ret = 0;
    uint16_t messageid = _femto_msg.header.femto_header.messageid;

    FEMTO_DEBUG("FEMTO process_message messid=%u\n",messageid);

    if (messageid == FEMTO_MSG_ID_PX4GPS) /**< px4gpsB*/
    {
		memset(_gps_position,0,sizeof(struct vehicle_gps_position_s));
		memset(&_femto_px4_gps,0,sizeof(struct femto_px4_gps_t));

		memcpy(&_femto_px4_gps.time_utc_usec,&_femto_msg.data.bytes[0],sizeof(struct femto_px4_gps_t));

		_gps_position->time_utc_usec = _femto_px4_gps.time_utc_usec;
		_gps_position->lat = _femto_px4_gps.lat;
		_gps_position->lon = _femto_px4_gps.lon;
		_gps_position->alt = _femto_px4_gps.alt;
		_gps_position->alt_ellipsoid = _femto_px4_gps.alt_ellipsoid;
		_gps_position->s_variance_m_s = _femto_px4_gps.s_variance_m_s;
		_gps_position->c_variance_rad = _femto_px4_gps.c_variance_rad;
		_gps_position->eph = _femto_px4_gps.eph;
		_gps_position->epv = _femto_px4_gps.epv;
		_gps_position->hdop = _femto_px4_gps.hdop;
		_gps_position->vdop = _femto_px4_gps.vdop;
		_gps_position->noise_per_ms = _femto_px4_gps.noise_per_ms;
		_gps_position->jamming_indicator = _femto_px4_gps.jamming_indicator;
		_gps_position->vel_m_s = _femto_px4_gps.vel_m_s;
		_gps_position->vel_n_m_s = _femto_px4_gps.vel_n_m_s;
		_gps_position->vel_e_m_s = _femto_px4_gps.vel_e_m_s;
		_gps_position->vel_d_m_s = _femto_px4_gps.vel_d_m_s;
		_gps_position->cog_rad = _femto_px4_gps.cog_rad;
		_gps_position->timestamp_time_relative = _femto_px4_gps.timestamp_time_relative;
		_gps_position->heading = _femto_px4_gps.heading;
		_gps_position->fix_type = _femto_px4_gps.fix_type;
		_gps_position->vel_ned_valid = _femto_px4_gps.vel_ned_valid;
		_gps_position->satellites_used = _femto_px4_gps.satellites_used;

		_gps_position->timestamp = gps_absolute_time();

		ret = 1;
	}
	return ret;
}

void GPSDriverFemto::activateRTCMOutput()
{
	const char *config_options[][2] = {
		"LOG RTCM 1\r\n",     "<LOG OK"    		/**< log rtcm*/
		"SAVECONFIG\r\n",     "<SAVECONFIG OK"   	/**< saveconfig*/
	};

	for (unsigned int conf_i = 0; conf_i < sizeof(config_options) / sizeof(config_options[0]); conf_i++) {
		if (writeAckedCommandFemto(config_options[conf_i][0], config_options[conf_i][1], FEMO_RESPONSE_TIMEOUT) != 0) {
			FEMTO_DEBUG("command %s failed", config_options[conf_i][0]);
		}
	}
}

void GPSDriverFemto::receiveWait(unsigned timeout_min)
{
	gps_abstime time_started = gps_absolute_time();

	while (gps_absolute_time() < time_started + timeout_min * 1000) {
		receive(timeout_min);
	}

}

int GPSDriverFemto::receive(unsigned timeout)
{
		uint8_t buf[GPS_READ_BUFFER_SIZE] = "";

		/* timeout additional to poll */
		uint64_t time_started = gps_absolute_time();
		int j = 0;
		int bytes_count = 0;

		while (true) {

			/* pass received bytes to the packet decoder */
			while (j < bytes_count) {
				int l = 0;
				if ((l = parseChar(buf[j])) > 0) {
					/* return to configure during configuration or to the gps driver during normal work
					 * if a packet has arrived */
					int ret = handleMessage(l);

					if (ret > 0) {
						FEMTO_DEBUG("femtomes parse one packet success\n");
						_decode_state = FemtoDecodeState::pream_ble1;
						return ret;
					}
				}
				j++;
			}

			/* everything is read */
			j = bytes_count = 0;

			/* then poll or read for new data */
			int ret = read(buf, sizeof(buf), timeout * 2);

			if (ret < 0) {
				/* something went wrong when polling */
				return -1;

			} else if (ret == 0) {
				/* Timeout while polling or just nothing read if reading, let's
				 * stay here, and use timeout below. */

			} else{
				/* if we have new data from GPS, go handle it */
				bytes_count = ret;
			}

			/* in case we get crap from GPS or time out */
			if (time_started + timeout * 1000 * 4 < gps_absolute_time()) {
				FEMTO_DEBUG("femtomes timeout\n");
				return -1;
			}
		}
}

#define HEXDIGIT_CHAR(d) ((char)((d) + (((d) < 0xA) ? '0' : 'A'-0xA)))

int GPSDriverFemto::parseChar(uint8_t temp)
{
	int iRet = 0;

	switch (_decode_state)
	{
		case FemtoDecodeState::pream_ble1:
			if (temp == FEMTO_PREAMBLE1) {
				_decode_state = FemtoDecodeState::pream_ble2;
				_femto_msg.read = 0;
			}
			break;

		case FemtoDecodeState::pream_ble2:
			if (temp == FEMTO_PREAMBLE2)
			{
				_decode_state = FemtoDecodeState::pream_ble3;
			}
			else
			{
				_decode_state = FemtoDecodeState::pream_ble1;
			}
			break;

		case FemtoDecodeState::pream_ble3:
			if (temp == FEMTO_PREAMBLE3)
			{
				_decode_state = FemtoDecodeState::head_length;
			}
			else
			{
				_decode_state = FemtoDecodeState::pream_ble1;
			}
			break;

		case FemtoDecodeState::head_length:
			_femto_msg.header.data[0] = FEMTO_PREAMBLE1;
			_femto_msg.header.data[1] = FEMTO_PREAMBLE2;
			_femto_msg.header.data[2] = FEMTO_PREAMBLE3;
			_femto_msg.header.data[3] = temp;
			_femto_msg.header.femto_header.headerlength = temp;
			_decode_state = FemtoDecodeState::head_data;
			_femto_msg.read = 4;
			break;

		case FemtoDecodeState::head_data:
			if (_femto_msg.read >= sizeof(_femto_msg.header.data)) {
				_decode_state = FemtoDecodeState::pream_ble1;
				break;
			}
			_femto_msg.header.data[_femto_msg.read] = temp;
			_femto_msg.read++;
			if (_femto_msg.read >= _femto_msg.header.femto_header.headerlength)
			{
				_decode_state = FemtoDecodeState::data;
			}
			break;
		case FemtoDecodeState::data:
			if (_femto_msg.read >= sizeof(_femto_msg.data)) {
				_decode_state = FemtoDecodeState::pream_ble1;
				break;
			}
			_femto_msg.data.bytes[_femto_msg.read - _femto_msg.header.femto_header.headerlength] = temp;
			_femto_msg.read++;
			if (_femto_msg.read >= (_femto_msg.header.femto_header.messagelength + _femto_msg.header.femto_header.headerlength))
			{
				_decode_state = FemtoDecodeState::crc1;
			}
			break;
		case FemtoDecodeState::crc1:
			_femto_msg.crc = (uint32_t) (temp << 0);
			_decode_state = FemtoDecodeState::crc2;
			break;
		case FemtoDecodeState::crc2:
			_femto_msg.crc += (uint32_t) (temp << 8);
			_decode_state = FemtoDecodeState::crc3;
			break;
		case FemtoDecodeState::crc3:
			_femto_msg.crc += (uint32_t) (temp << 16);
			_decode_state = FemtoDecodeState::crc4;
			break;
		case FemtoDecodeState::crc4:
			_femto_msg.crc += (uint32_t) (temp << 24);
			_decode_state = FemtoDecodeState::pream_ble1;

			uint32_t crc = CalculateBlockCRC32((uint32_t)_femto_msg.header.femto_header.headerlength, (uint8_t *)&_femto_msg.header.data, (uint32_t)0);
			crc = CalculateBlockCRC32((uint32_t)_femto_msg.header.femto_header.messagelength, (uint8_t *)&_femto_msg.data, crc);

			if (_femto_msg.crc == crc)
			{
				iRet = _femto_msg.read;
				FEMTO_DEBUG("data packet is compelete");
			}
			else
			{
				FEMTO_DEBUG("data packet is bad");
			}
			break;
	}
	return iRet;
}

void GPSDriverFemto::decodeInit()
{
	_decode_state = FemtoDecodeState::pream_ble1;

	if (_output_mode == OutputMode::RTCM) {
		if (!_rtcm_parsing) {
			_rtcm_parsing = new RTCMParsing();
		}
		_rtcm_parsing->reset();
	}

}

int GPSDriverFemto::writeAckedCommandFemto(const char* command, const char* reply, const unsigned int timeout)
{
	/**< write command*/
	write(command, strlen(command));
	/**< wait for reply*/
	uint8_t buf[GPS_READ_BUFFER_SIZE] = "";
	gps_abstime time_started = gps_absolute_time();
	while (time_started + timeout * 1000 * 2 > gps_absolute_time()) {
		int ret = read(buf, sizeof(buf), 1000); /**< wait 1000us */
		if (ret > 0 && strstr((char *)buf, reply) != NULL) {
			FEMTO_DEBUG("command reply success: %s", command);
			return 0;
		}
	}
	return -1;

}

int GPSDriverFemto::configure(unsigned &baudrate,OutputMode output_mode)
{
	_output_mode = output_mode;
	_correction_output_activated = false;
	_configure_done = false;

	/** Try different baudrates (115200 is the default for Femtomes) and request the baudrate that we want.	 */
	const unsigned baudrates_to_try[] = {9600, 38400, 19200, 57600, 115200};
	bool success = false;

	unsigned test_baudrate;

	for (unsigned int baud_i = 0; !success && baud_i < sizeof(baudrates_to_try) / sizeof(baudrates_to_try[0]); baud_i++) {
		test_baudrate = baudrates_to_try[baud_i];

		if (baudrate > 0 && baudrate != test_baudrate) {
			continue; /**< skip to next baudrate*/
		}

		setBaudrate(test_baudrate);

		FEMTO_DEBUG("baudrate set to %i", test_baudrate);

		for (int run = 0; run < 2; ++run) { /** try several times*/
			if (writeAckedCommandFemto("UNLOGALL\r\n", "<UNLOGALL OK", FEMO_RESPONSE_TIMEOUT) == 0 &&
				writeAckedCommandFemto("VERSION\r\n", "<VERSION OK", FEMO_RESPONSE_TIMEOUT) == 0) {
				FEMTO_DEBUG("got port for baudrate %i", test_baudrate);
				success = true;
				break;
			}
		}
	}

	if (!success) {
		FEMTO_DEBUG("femtomes gps start failed %i", test_baudrate);
		return -1;
	}
	/**
	* We successfully got a response and know to which port we are connected. Now set the desired baudrate
	* if it's different from the current one.
	*/
	const unsigned desired_baudrate = 115200; /**< changing this requires also changing the SPD command*/

	baudrate = test_baudrate;

	if (baudrate != desired_baudrate) {
		baudrate = desired_baudrate;
		//@TODO
		const char baud_config[] = "com%c 115200\r\n"; // configure baudrate to 115200
		char baud_config_str[sizeof(baud_config)];
		int len = snprintf(baud_config_str, sizeof(baud_config_str), baud_config, _port);
		write(baud_config_str, len);
		decodeInit();
		receiveWait(200);
		decodeInit();
		setBaudrate(baudrate);

		success = false;

		for (int run = 0; run < 10; ++run) {
			/** We ask for the port config again. If we get a reply, we know that the changed settings work.*/
			if (writeAckedCommandFemto("UNLOGALL\r\n", "<UNLOGALL OK",FEMO_RESPONSE_TIMEOUT) == 0 &&
				writeAckedCommandFemto("VERSION\r\n", "<VERSION OK",FEMO_RESPONSE_TIMEOUT) == 0) {
				success = true;
				break;
			}
		}

		if (!success) {
			return -1;
		}

		FEMTO_DEBUG("Successfully configured the baudrate");
	}
	PX4_INFO("Successfully configured the baudrate");
#if 0
	const char *config_options[][2] = {
		"UNLOGALL\r\n",     "<UNLOGALL OK"    		/**< disable all NMEA and NMEA-Like Messages*/
		"LOG PX4GPSB 0.2\r\n", "<LOG OK"   	/**< disable all ATM (ATOM) Messages*/
	};

	for (unsigned int conf_i = 0; conf_i < sizeof(config_options) / sizeof(config_options[0]); conf_i++) {
		if (writeAckedCommandFemto(config_options[conf_i][0], config_options[conf_i][1], FEMO_RESPONSE_TIMEOUT) != 0) {
			FEMTO_DEBUG("command %s failed", config_options[conf_i][0]);
		}
	}
#endif
	if (writeAckedCommandFemto("LOG PX4GPSB 0.2\r\n", "<LOG OK",FEMO_RESPONSE_TIMEOUT) == 0){
		FEMTO_DEBUG("command LOG PX4GPSB 0.2 success");
	}else{
		FEMTO_DEBUG("command LOG PX4GPSB 0.2 failed");
	}

	if (output_mode == OutputMode::RTCM && _board == FemtoBoardType::BT_6A0) {
		SurveyInStatus status;
		status.latitude = status.longitude = (double)NAN;
		status.altitude = NAN;
		status.duration = 0;
		status.mean_accuracy = 0;
		const bool valid = false;
		const bool active = true;
		status.flags = (int)valid | ((int)active << 1);
		surveyInStatus(status);
	}

	_configure_done = true;
	return 0;
}

void GPSDriverFemto::activateCorrectionOutput()
{
	if (_correction_output_activated || _output_mode != OutputMode::RTCM) {
		return;
	}

	_correction_output_activated = true;
	char buffer[100];

	if (_base_settings.type == BaseSettingsType::survey_in) {
		FEMTO_DEBUG("enabling survey-in");
		if(writeAckedCommandFemto("POSAVE AUTO\r\n","<POSAVE OK",FEMO_RESPONSE_TIMEOUT) == 0)
		{
			FEMTO_DEBUG("POSEAVE OK");
			_base_settings.settings.survey_in.min_dur = 0; // use it as counter how long survey-in has been active
			_survey_in_start = gps_absolute_time();
			sendSurveyInStatusUpdate(true, false);
		}

	} else {
		FEMTO_DEBUG("setting base station position");

		const FixedPositionSettings &settings = _base_settings.settings.fixed_position;

		int len = snprintf(buffer, sizeof(buffer), "FIX POSITION %.8lf %.8lf %.5f\r\n",
				   settings.latitude, settings.longitude,(double)settings.altitude);

		if (len >= 0 && len < (int)sizeof(buffer)) {
			if(writeAckedCommandFemto("POSAVE AUTO\r\n","<FIX OK",FEMO_RESPONSE_TIMEOUT) == 0)
			{
				FEMTO_DEBUG("FIX OK");
				_base_settings.settings.survey_in.min_dur = 0; // use it as counter how long survey-in has been active
				_survey_in_start = gps_absolute_time();
				sendSurveyInStatusUpdate(true, false);
			}
		}

		activateRTCMOutput();
		sendSurveyInStatusUpdate(false, true, settings.latitude, settings.longitude, settings.altitude);
	}
}

void
GPSDriverFemto::sendSurveyInStatusUpdate(bool active, bool valid, double latitude, double longitude, float altitude)
{
	SurveyInStatus status;
	status.latitude = latitude;
	status.longitude = longitude;
	status.altitude = altitude;
	status.duration = _base_settings.settings.survey_in.min_dur;
	status.mean_accuracy = 0; /**< unknown*/
	status.flags = (int)valid | ((int)active << 1);
	surveyInStatus(status);
}

#define CRC32_POLYNOMIAL 0xEDB88320L
uint32_t
GPSDriverFemto::CRC32Value(uint32_t icrc)
{
    int i;
    uint32_t crc = icrc;
    for ( i = 8 ; i > 0; i-- )
    {
        if ( crc & 1 )
            crc = ( crc >> 1 ) ^ CRC32_POLYNOMIAL;
        else
            crc >>= 1;
    }
    return crc;
}

uint32_t
GPSDriverFemto::CalculateBlockCRC32(uint32_t length, uint8_t *buffer, uint32_t crc)
{
    while ( length-- != 0 )
    {
        crc = ((crc >> 8) & 0x00FFFFFFL) ^ (CRC32Value(((uint32_t) crc ^ *buffer++) & 0xff));
    }
    return( crc );
}
