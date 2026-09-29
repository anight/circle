//
// usbcdcgadgetendpoint.cpp
//
// This file by Sebastien Nicolas <seba1978@gmx.de>
//
// Circle - A C++ bare metal environment for Raspberry Pi
// Copyright (C) 2023-2025  R. Stange <rsta2@gmx.net>
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//
#include <circle/usb/gadget/usbcdcgadgetendpoint.h>
#include <circle/usb/gadget/usbcdcgadget.h>
#include <circle/usb/usbserial.h>
#include <circle/util.h>
#include <assert.h>

CUSBCDCGadgetEndpoint::CUSBCDCGadgetEndpoint (const TUSBEndpointDescriptor *pDesc,
						CDWUSBGadget *pGadget)
:	CDWUSBGadgetEndpoint (pDesc, pGadget),
	m_pInterface (nullptr),
	m_nStatus (0),
	m_bInActive (FALSE),
	m_pQueue (nullptr),
	m_nInPtr (0),
	m_nOutPtr (0)
{
	m_pQueue = new u8[QueueSize];
	assert (m_pQueue);
}

CUSBCDCGadgetEndpoint::~CUSBCDCGadgetEndpoint (void)
{
	delete [] m_pQueue;
	m_pQueue = nullptr;
}

void CUSBCDCGadgetEndpoint::AttachInterface (CUSBSerialDevice *pInterface)
{
	m_nStatus = 0;

	assert (!m_pInterface);
	m_pInterface = pInterface;
	assert (m_pInterface);

	if (GetDirection () == DirectionIn)
	{
		m_pInterface->RegisterWriteHandler (WriteHandler, this);
	}
	else
	{
		m_pInterface->RegisterReadHandler (ReadHandler, this);
	}
}

void CUSBCDCGadgetEndpoint::OnActivate (void)
{
	if (GetDirection () == DirectionOut)
	{
		BeginTransfer (TransferDataOut, m_OutBuffer, MaxOutMessageSize);
	}
}

void CUSBCDCGadgetEndpoint::OnDeactivate (void)
{
	if (GetDirection () == DirectionOut)
	{
		CancelTransfer ();
	}
}

void CUSBCDCGadgetEndpoint::OnTransferComplete (boolean bIn, size_t nLength)
{
	if (!bIn)
	{
		m_SpinLock.Acquire ();

		unsigned nBytesFree = GetQueueBytesFree ();
		if (nLength > nBytesFree)
		{
			nLength = nBytesFree;

			m_nStatus = -1;		// RX overrun
		}

		if (nLength)			// (queue full: Enqueue asserts on 0)
		{
			Enqueue (m_OutBuffer, nLength);
		}

		m_SpinLock.Release ();

		BeginTransfer (TransferDataOut, m_OutBuffer, MaxOutMessageSize);
	}
	else
	{
		m_SpinLock.Acquire ();

		if (m_bInActive)
		{
			unsigned nBytesAvail = GetQueueBytesAvail ();
			if (nBytesAvail)
			{
				nBytesAvail = InTransferLength (nBytesAvail);	// see Write()

				Dequeue (m_InBuffer, nBytesAvail);

				BeginTransfer (TransferDataIn, m_InBuffer, nBytesAvail);
			}
			else
			{
				m_bInActive = FALSE;
			}
		}

		m_SpinLock.Release ();
	}
}

void CUSBCDCGadgetEndpoint::OnSuspend (void)
{
	if (GetDirection () == DirectionIn)
	{
		m_SpinLock.Acquire ();

		m_bInActive = FALSE;
		m_nInPtr = 0;
		m_nOutPtr = 0;

		m_SpinLock.Release ();
	}
}

int CUSBCDCGadgetEndpoint::Write (const void *pData, unsigned nLength)
{
	assert (pData != 0);
	assert (nLength > 0);

	m_SpinLock.Acquire ();

	if (m_nStatus)
	{
		int nStatus = m_nStatus;
		m_nStatus = 0;

		m_SpinLock.Release ();

		return nStatus;
	}

	unsigned nBytesFree = GetQueueBytesFree ();
	if (!nBytesFree)
	{
		m_SpinLock.Release ();

		return 0;
	}

	if (nLength > nBytesFree)
	{
		nLength = nBytesFree;
	}

	Enqueue (pData, nLength);

	if (m_bInActive)
	{
		m_SpinLock.Release ();

		return nLength;
	}

	m_bInActive = TRUE;

	unsigned nBytesAvail = GetQueueBytesAvail ();
	nBytesAvail = InTransferLength (nBytesAvail);

	Dequeue (m_InBuffer, nBytesAvail);

	m_SpinLock.Release ();

	BeginTransfer (TransferDataIn, m_InBuffer, nBytesAvail);

	return nLength;
}

// pigpu: every transfer ends with a short packet: a full-size last packet
// without a following ZLP leaves the data pending in the host's (cdc_acm) read
// URB. So never a multiple of 64 bytes (the full-speed packet size; then not
// of 512 either).
unsigned CUSBCDCGadgetEndpoint::InTransferLength (unsigned nBytesAvail)
{
	if (nBytesAvail > MaxInMessageSize)
	{
		nBytesAvail = MaxInMessageSize;
	}
	if (nBytesAvail % 64 == 0)
	{
		nBytesAvail--;
	}

	return nBytesAvail;
}

int CUSBCDCGadgetEndpoint::Read (void *pBuffer, unsigned nLength)
{
	m_SpinLock.Acquire ();

	if (m_nStatus)
	{
		int nStatus = m_nStatus;
		m_nStatus = 0;

		m_SpinLock.Release ();

		return nStatus;
	}

	unsigned nBytesAvail = GetQueueBytesAvail ();
	if (!nBytesAvail)
	{
		m_SpinLock.Release ();

		return 0;
	}

	if (nBytesAvail > nLength)
	{
		nBytesAvail = nLength;
	}

	Dequeue (pBuffer, nBytesAvail);

	m_SpinLock.Release ();

	return nBytesAvail;
}

int CUSBCDCGadgetEndpoint::WriteHandler (const void *pBuffer, size_t nCount, void *pParam)
{
	CUSBCDCGadgetEndpoint *pThis = static_cast<CUSBCDCGadgetEndpoint *> (pParam);
	assert (pThis);

	return pThis->Write (pBuffer, nCount);
}

int CUSBCDCGadgetEndpoint::ReadHandler (void *pBuffer, size_t nCount, void *pParam)
{
	CUSBCDCGadgetEndpoint *pThis = static_cast<CUSBCDCGadgetEndpoint *> (pParam);
	assert (pThis);

	return pThis->Read (pBuffer, nCount);
}

unsigned CUSBCDCGadgetEndpoint::GetQueueBytesFree (void)
{
	assert (m_nInPtr < QueueSize);
	assert (m_nOutPtr < QueueSize);

	if (m_nOutPtr <= m_nInPtr)
	{
		return QueueSize+m_nOutPtr-m_nInPtr-1;
	}

	return m_nOutPtr-m_nInPtr-1;
}

unsigned CUSBCDCGadgetEndpoint::GetQueueBytesAvail (void)
{
	assert (m_nInPtr < QueueSize);
	assert (m_nOutPtr < QueueSize);

	if (m_nInPtr < m_nOutPtr)
	{
		return QueueSize+m_nInPtr-m_nOutPtr;
	}

	return m_nInPtr-m_nOutPtr;
}

void CUSBCDCGadgetEndpoint::Enqueue (const void *pBuffer, unsigned nCount)
{
	const u8 *p = static_cast<const u8 *> (pBuffer);
	assert (p != 0);
	assert (m_pQueue != 0);

	assert (nCount > 0);
	// pigpu: in blocks (up to the end of the ring, then from its start)
	unsigned nInPtr = m_nInPtr;
	while (nCount > 0)
	{
		unsigned nChunk = QueueSize - nInPtr < nCount ? QueueSize - nInPtr : nCount;
		memcpy (m_pQueue + nInPtr, p, nChunk);
		p += nChunk;
		nCount -= nChunk;
		nInPtr += nChunk;
		if (nInPtr == QueueSize)
		{
			nInPtr = 0;
		}
	}
	m_nInPtr = nInPtr;
}

void CUSBCDCGadgetEndpoint::Dequeue (void *pBuffer, unsigned nCount)
{
	u8 *p = static_cast<u8 *> (pBuffer);
	assert (p != 0);
	assert (m_pQueue != 0);

	assert (nCount > 0);
	// pigpu: in blocks (up to the end of the ring, then from its start)
	unsigned nOutPtr = m_nOutPtr;
	while (nCount > 0)
	{
		unsigned nChunk = QueueSize - nOutPtr < nCount ? QueueSize - nOutPtr : nCount;
		memcpy (p, m_pQueue + nOutPtr, nChunk);
		p += nChunk;
		nCount -= nChunk;
		nOutPtr += nChunk;
		if (nOutPtr == QueueSize)
		{
			nOutPtr = 0;
		}
	}
	m_nOutPtr = nOutPtr;
}
