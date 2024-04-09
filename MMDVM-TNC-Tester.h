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

#if !defined(MMDVMTNCTESTER_H)
#define	MMDVMTNCTESTER_H

#include "UARTController.h"

class CMMDVM_TNC_Tester {
public:
	CMMDVM_TNC_Tester(const std::string& port, unsigned int speed, const std::string& src, const std::string& dest, unsigned int count, const std::string& payloadText, unsigned int payloadLen, unsigned int interval);
	~CMMDVM_TNC_Tester();

	int run();

private:
	CUARTController m_serial;
	std::string  m_src;
	std::string  m_dest;
	unsigned int m_count;
	std::string  m_payloadText;
	unsigned int m_payloadLen;
	unsigned int m_interval;
	uint8_t      m_buffer[2000U];
	unsigned int m_ptr;
	bool         m_inFrame;
	bool         m_isEscaped;

	bool transmit(unsigned int n);
	bool receive();

	bool writeKISS(const uint8_t* frame, size_t length);
	void process();
	bool decodeAddress(const uint8_t* data, std::string& text, bool isDigi = false) const;
	void encodeAddress(uint8_t* data, const std::string& callsign, bool command, bool end) const;
	void dump(const uint8_t* data, size_t length) const;
};

#endif
