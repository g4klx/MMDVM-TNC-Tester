/*
 *   Copyright (C) 2024 by Jonathan Naylor G4KLX
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "MMDVM-TNC-Tester.h"
#include "StopWatch.h"
#include "Thread.h"

#include "KISSDefines.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>

int main(int argc, char** argv)
{
	std::string src = "A0AAA";
	std::string dest = "B0BBB";
	unsigned int count = 1000U;
	std::string payloadText = "test";
	unsigned int payloadLen = 25U;
	unsigned int interval = 5U;

	if (argc < 3) {
		::fprintf(stderr, "Usage: mmdvm-tnc-tester <port> <speed> [src call] [dest call] [count] [payload text] [payload length] [interval]\n");
		return 1;
	}

	std::string port = std::string(argv[1]);

	unsigned int speed = ::atoi(argv[2]);

	if (argc > 4)
		count = ::atoi(argv[3]);

	if (argc > 5)
		payloadText = std::string(argv[4]);

	if (argc > 6)
		payloadLen = ::atoi(argv[5]);

	if (argc > 7)
		interval = ::atoi(argv[6]);

	CMMDVM_TNC_Tester tester(port, speed, src, dest, count, payloadText, payloadLen, interval);

	return tester.run();
}

CMMDVM_TNC_Tester::CMMDVM_TNC_Tester(const std::string& port, unsigned int speed, const std::string& src, const std::string& dest, unsigned int count, const std::string& payloadText, unsigned int payloadLen, unsigned int interval) :
m_serial(port, speed),
m_src(src),
m_dest(dest),
m_count(count),
m_payloadText(payloadText),
m_payloadLen(payloadLen),
m_interval(interval),
m_buffer(),
m_ptr(0U),
m_inFrame(false),
m_isEscaped(false)
{
	assert(!port.empty());
	assert(speed > 0U);
	assert(!src.empty());
	assert(!dest.empty());
	assert(count > 0U);
	assert(!payloadText.empty());
	assert(payloadLen > 0U);
	assert(interval > 0U);
}

CMMDVM_TNC_Tester::~CMMDVM_TNC_Tester()
{
}

int CMMDVM_TNC_Tester::run()
{
	bool ret = m_serial.open();
	if (!ret)
		return 1;

	CStopWatch stopwatch;
	stopwatch.start();

	unsigned int count = 0U;
	while (count < m_count) {
		if (stopwatch.elapsed() >= (m_interval * 100U)) {
			ret = transmit(count);
			if (!ret)
				break;

			stopwatch.start();
			count++;
		}

		ret = receive();
		if (!ret)
			break;

		CThread::sleep(10U);
	}

	m_serial.close();

	return 0;
}

bool CMMDVM_TNC_Tester::transmit(unsigned int n)
{
	uint8_t frame[300U];

	// <UI C>
	encodeAddress(frame + 0U, m_dest, true, false);
	encodeAddress(frame + 7U, m_src, false, true);

	frame[14U] = 0x03U;
	frame[15U] = 0xF0U;		// No L3

	size_t length = 16U;

	char payload[200U];
	::sprintf(payload, "%s%u ", m_payloadText.c_str(), n + 1U);

	::memcpy(frame + length, payload, ::strlen(payload));
	length += ::strlen(payload);

	if (::strlen(payload) < m_payloadLen) {
		size_t extra = m_payloadLen - ::strlen(payload);
		for (size_t i = 0U; i < extra; i++)
			frame[length++] = (::rand() % 94) + 32U;
	}

	return writeKISS(frame, length);
}

bool CMMDVM_TNC_Tester::receive()
{
	for (;;) {
		uint8_t c = 0U;
		int16_t ret = m_serial.read(&c, 1U);
		if (ret == 0)
			return true;
		if (ret < 0)
			return false;

		if (!m_inFrame) {
			if (c == KISS_FEND) {
				// Handle the frame start
				m_inFrame = true;
				m_isEscaped = false;
				m_ptr = 0U;
			}
		} else {
			// Any other bytes are added to the buffer-ish
			switch (c) {
			case KISS_TFESC:
				m_buffer[m_ptr++] = m_isEscaped ? KISS_FESC : KISS_TFESC;
				m_isEscaped = false;
				break;
			case KISS_TFEND:
				m_buffer[m_ptr++] = m_isEscaped ? KISS_FEND : KISS_TFEND;
				m_isEscaped = false;
				break;
			case KISS_FESC:
				m_isEscaped = true;
				break;
			case KISS_FEND:
				if (m_ptr > 0U)
					process();
				m_inFrame = false;
				m_isEscaped = false;
				m_ptr = 0U;
				break;
			default:
				m_buffer[m_ptr++] = c;
				break;
			}
		}
	}
}

void CMMDVM_TNC_Tester::process()
{
	// dump("Data from TNC", m_buffer, m_ptr);

	if (m_buffer[0U] != KISS_TYPE_DATA) {
		::fprintf(stdout, "KISS message of type 0x%02X received\n", m_buffer[0U]);
		return;
	}

	std::string text;

	bool more = decodeAddress(m_buffer + 8U, text);

	text += '>';

	decodeAddress(m_buffer + 1U, text);

	unsigned int n = 15U;
	while (more && n < m_ptr) {
		text += ',';
		more = decodeAddress(m_buffer + n, text, true);
		n += 7U;
	}

	text += ' ';

	std::string cr = "";
	if (((m_buffer[7U] & 0x80U) == 0x80U) && ((m_buffer[14U] & 0x80U) == 0x00U))
		cr = " C";
	if (((m_buffer[7U] & 0x80U) == 0x00U) && ((m_buffer[14U] & 0x80U) == 0x80U))
		cr = " R";

	if ((m_buffer[n] & 0x01U) == 0x00U) {
		std::string pf = "";
		if (cr == " C")
			pf = (m_buffer[n] & 0x10U) == 0x10U ? " P" : "";
		else if (cr == " R")
			pf = (m_buffer[n] & 0x10U) == 0x10U ? " F" : "";

		// I frame
		char t[20U];
		::sprintf(t, "<I%s%s S%u R%u>", cr.c_str(), pf.c_str(), (m_buffer[n] >> 1) & 0x07U, (m_buffer[n] >> 5) & 0x07U);
		text += t;
	} else {
		if ((m_buffer[n] & 0x02U) == 0x00U) {
			std::string pf = "";
			if (cr == " C")
				pf = (m_buffer[n] & 0x10U) == 0x10U ? " P" : "";
			else if (cr == " R")
				pf = (m_buffer[n] & 0x10U) == 0x10U ? " F" : "";

			// S frame
			char t[20U];
			switch (m_buffer[n] & 0x0FU) {
			case 0x01U:
				sprintf(t, "<RR%s%s R%u>", cr.c_str(), pf.c_str(), (m_buffer[n] >> 5) & 0x07U);
				break;
			case 0x05U:
				sprintf(t, "<RNR%s%s R%u>", cr.c_str(), pf.c_str(), (m_buffer[n] >> 5) & 0x07U);
				break;
			case 0x09U:
				sprintf(t, "<REJ%s%s R%u>", cr.c_str(), pf.c_str(), (m_buffer[n] >> 5) & 0x07U);
				break;
			case 0x0DU:
				sprintf(t, "<SREJ%s%s R%u>", cr.c_str(), pf.c_str(), (m_buffer[n] >> 5) & 0x07U);
				break;
			default:
				sprintf(t, "<Unknown%s%s R%u>", cr.c_str(), pf.c_str(), (m_buffer[n] >> 5) & 0x07U);
				break;
			}

			text += t;
			::fprintf(stderr, "%s\n", text.c_str());
			return;
		} else {
			std::string pf = "";
			if (cr == " C")
				pf = (m_buffer[n] & 0x10U) == 0x10U ? " P" : "";
			else if (cr == " R")
				pf = (m_buffer[n] & 0x10U) == 0x10U ? " F" : "";

			// U frame
			char t[20U];
			switch (m_buffer[n] & 0xEFU) {
			case 0x6FU:
				sprintf(t, "<SABME%s%s>", cr.c_str(), pf.c_str());
				break;
			case 0x2FU:
				sprintf(t, "<SABM%s%s>", cr.c_str(), pf.c_str());
				break;
			case 0x43U:
				sprintf(t, "<DISC%s%s>", cr.c_str(), pf.c_str());
				break;
			case 0x0FU:
				sprintf(t, "<DM%s%s>", cr.c_str(), pf.c_str());
				break;
			case 0x63U:
				sprintf(t, "<UA%s%s>", cr.c_str(), pf.c_str());
				break;
			case 0x87U:
				sprintf(t, "<FRMR%s%s>", cr.c_str(), pf.c_str());
				break;
			case 0x03U:
				sprintf(t, "<UI%s%s>", cr.c_str(), pf.c_str());
				break;
			case 0xAFU:
				sprintf(t, "<XID%s%s>", cr.c_str(), pf.c_str());
				break;
			case 0xE3U:
				sprintf(t, "<TEST%s%s>", cr.c_str(), pf.c_str());
				break;
			default:
				sprintf(t, "<Unknown%s%s>", cr.c_str(), pf.c_str());
				break;
			}

			text += t;

			if ((m_buffer[n] & 0xEFU) != 0x03U) {
				::fprintf(stderr, "%s\n", text.c_str());
				return;
			}
		}
	}

	n += 2U;

	::fprintf(stderr, "%s %.*s\n", text.c_str(), m_ptr - n, m_buffer + n);
}

bool CMMDVM_TNC_Tester::decodeAddress(const uint8_t* data, std::string& text, bool isDigi) const
{
	assert(data != nullptr);

	for (unsigned int i = 0U; i < 6U; i++) {
		uint8_t c = data[i] >> 1;
		if (c != ' ')
			text += c;
	}

	uint8_t ssid = (data[6U] >> 1) & 0x0FU;
	if (ssid > 0U) {
		text += '-';
		if (ssid >= 10U) {
			text += '1';
			text += '0' + ssid - 10U;
		} else {
			text += '0' + ssid;
		}
	}

	if (isDigi) {
		if ((data[6U] & 0x80U) == 0x80U)
			text += '*';
	}

	return (data[6U] & 0x01U) == 0x00U;
}

void CMMDVM_TNC_Tester::encodeAddress(uint8_t* data, const std::string& callsign, bool command, bool end) const
{
	assert(data != nullptr);
	assert(!callsign.empty());

	size_t n = callsign.find_first_of('-');

	size_t length = callsign.length();
	if (n != std::string::npos)
		length = n;
	if (length > 6U)
		length = 6U;

	for (unsigned int i = 0U; i < 6U; i++)
		data[i] = ' ' << 1;

	for (size_t i = 0U; i < length; i++)
		data[i] = callsign[i] << 1;

	data[6U] = 0x60U;		// SSID of 0

	if (command)
		data[6U] |= 0x80U;

	if (n != std::string::npos) {
		int ssid = std::stoi(callsign.substr(n + 1U));
		data[6U] |= ssid << 1;
	}

	if (end)
		data[6U] |= 0x01U;
}

bool CMMDVM_TNC_Tester::writeKISS(const uint8_t* frame, size_t length)
{
	assert(frame != nullptr);
	assert(length > 0U);

	// dump("Data to TNC", frame, length);

	uint8_t buffer[2U];

	buffer[0U] = KISS_FEND;
	buffer[1U] = KISS_TYPE_DATA;
	int16_t ret = m_serial.write(buffer, 2U);
	if (ret != 2)
		return false;

	for (size_t i = 0U; i < length; i++) {
		buffer[0U] = frame[i];

		switch (buffer[0U]) {
		case KISS_FEND:
			buffer[0U] = KISS_FESC;
			buffer[1U] = KISS_TFEND;
			ret = m_serial.write(buffer, 2U);
			if (ret != 2)
				return false;
			break;
		case KISS_FESC:
			buffer[0U] = KISS_FESC;
			buffer[1U] = KISS_TFESC;
			ret = m_serial.write(buffer, 2U);
			if (ret != 2)
				return false;
			break;
		default:
			ret = m_serial.write(buffer, 1U);
			if (ret != 1)
				return false;
			break;
		}
	}

	buffer[0U] = KISS_FEND;
	ret = m_serial.write(buffer, 1U);
	if (ret != 1)
		return false;

	return true;
}

void CMMDVM_TNC_Tester::dump(const char* title, const uint8_t* buffer, size_t length) const
{
	assert(title != nullptr);
	assert(buffer != nullptr);
	assert(length > 0U);

	::fprintf(stdout, "%s\n", title);

	size_t offset = 0U;

	while (offset < length) {
		::fprintf(stdout, "%04X: ", (unsigned int)offset);

		for (unsigned int i = 0U; i < 16U; i++) {
			if ((offset + i) < length)
				::fprintf(stdout, "%02X ", buffer[offset + i]);
			else
				::fprintf(stdout, "   ");
		}

		::fprintf(stdout, "  *");

		for (unsigned int i = 0U; i < 16U; i++) {
			if ((offset + i) < length) {
				if (isprint(buffer[offset + i]))
					::fprintf(stdout, "%c", buffer[offset + i]);
				else
					::fprintf(stdout, ".");
			}
		}

		::fprintf(stdout, "*\n");

		offset += 16U;
	}
}
