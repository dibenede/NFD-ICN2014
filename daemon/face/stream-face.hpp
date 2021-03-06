/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/**
 * Copyright (c) 2014,  Regents of the University of California,
 *                      Arizona Board of Regents,
 *                      Colorado State University,
 *                      University Pierre & Marie Curie, Sorbonne University,
 *                      Washington University in St. Louis,
 *                      Beijing Institute of Technology,
 *                      The University of Memphis
 *
 * This file is part of NFD (Named Data Networking Forwarding Daemon).
 * See AUTHORS.md for complete list of NFD authors and contributors.
 *
 * NFD is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Foundation,
 * either version 3 of the License, or (at your option) any later version.
 *
 * NFD is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * NFD, e.g., in COPYING.md file.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NFD_DAEMON_FACE_STREAM_FACE_HPP
#define NFD_DAEMON_FACE_STREAM_FACE_HPP

#include "face.hpp"
#include "local-face.hpp"
#include "core/logger.hpp"

namespace nfd {

// forward declaration
template<class T, class U, class V> struct StreamFaceSenderImpl;

template<class Protocol, class FaceBase = Face>
class StreamFace : public FaceBase
{
public:
  typedef Protocol protocol;

  /**
   * \brief Create instance of StreamFace
   */
  StreamFace(const FaceUri& remoteUri, const FaceUri& localUri,
             const shared_ptr<typename protocol::socket>& socket,
             bool isOnDemand);

  virtual
  ~StreamFace();

  // from Face
  virtual void
  sendInterest(const Interest& interest);

  virtual void
  sendData(const Data& data);

  virtual void
  close();

protected:
  void
  processErrorCode(const boost::system::error_code& error);

  void
  sendFromQueue();

  void
  handleSend(const boost::system::error_code& error,
             size_t nBytesSent);

  void
  handleReceive(const boost::system::error_code& error,
                size_t nBytesReceived);

  void
  keepFaceAliveUntilAllHandlersExecuted(const shared_ptr<Face>& face);

  void
  closeSocket();

protected:
  shared_ptr<typename protocol::socket> m_socket;

private:
  uint8_t m_inputBuffer[MAX_NDN_PACKET_SIZE];
  size_t m_inputBufferSize;
  std::queue<Block> m_sendQueue;

  friend struct StreamFaceSenderImpl<Protocol, FaceBase, Interest>;
  friend struct StreamFaceSenderImpl<Protocol, FaceBase, Data>;

  NFD_LOG_INCLASS_DECLARE();
};

// All inherited classes must use
// NFD_LOG_INCLASS_TEMPLATE_SPECIALIZATION_DEFINE(StreamFace, <specialization-parameter>, "Name");


/** \brief Class allowing validation of the StreamFace use
 *
 *  For example, partial specialization based on boost::asio::ip::tcp should check
 *  that local endpoint is loopback
 *
 *  @throws Face::Error if validation failed
 */
template<class Protocol, class U>
struct StreamFaceValidator
{
  static void
  validateSocket(typename Protocol::socket& socket)
  {
  }
};


template<class T, class FaceBase>
inline
StreamFace<T, FaceBase>::StreamFace(const FaceUri& remoteUri, const FaceUri& localUri,
                const shared_ptr<typename StreamFace::protocol::socket>& socket,
                bool isOnDemand)
  : FaceBase(remoteUri, localUri)
  , m_socket(socket)
  , m_inputBufferSize(0)
{
  FaceBase::setOnDemand(isOnDemand);
  StreamFaceValidator<T, FaceBase>::validateSocket(*socket);
  m_socket->async_receive(boost::asio::buffer(m_inputBuffer, MAX_NDN_PACKET_SIZE), 0,
                          bind(&StreamFace<T, FaceBase>::handleReceive, this, _1, _2));
}

template<class T, class U>
inline
StreamFace<T, U>::~StreamFace()
{
}

template<class Protocol, class FaceBase, class Packet>
struct StreamFaceSenderImpl
{
  static void
  send(StreamFace<Protocol, FaceBase>& face, const Packet& packet)
  {
    bool wasQueueEmpty = face.m_sendQueue.empty();
    face.m_sendQueue.push(packet.wireEncode());

    if (wasQueueEmpty)
      face.sendFromQueue();
  }
};

// partial specialization (only classes can be partially specialized)
template<class Protocol, class Packet>
struct StreamFaceSenderImpl<Protocol, LocalFace, Packet>
{
  static void
  send(StreamFace<Protocol, LocalFace>& face, const Packet& packet)
  {
    bool wasQueueEmpty = face.m_sendQueue.empty();

    if (!face.isEmptyFilteredLocalControlHeader(packet.getLocalControlHeader()))
      {
        face.m_sendQueue.push(face.filterAndEncodeLocalControlHeader(packet));
      }
    face.m_sendQueue.push(packet.wireEncode());

    if (wasQueueEmpty)
      face.sendFromQueue();
  }
};


template<class T, class U>
inline void
StreamFace<T, U>::sendInterest(const Interest& interest)
{
  this->onSendInterest(interest);
  StreamFaceSenderImpl<T, U, Interest>::send(*this, interest);
}

template<class T, class U>
inline void
StreamFace<T, U>::sendData(const Data& data)
{
  this->onSendData(data);
  StreamFaceSenderImpl<T, U, Data>::send(*this, data);
}

template<class T, class U>
inline void
StreamFace<T, U>::sendFromQueue()
{
  const Block& block = this->m_sendQueue.front();
  boost::asio::async_write(*this->m_socket, boost::asio::buffer(block),
                           bind(&StreamFace<T, U>::handleSend, this, _1, _2));
}

template<class T, class U>
inline void
StreamFace<T, U>::close()
{
  if (!m_socket->is_open())
    return;

  NFD_LOG_INFO("[id:" << this->getId()
               << ",uri:" << this->getRemoteUri()
               << "] Close connection");

  closeSocket();
  this->fail("Close connection");
}

template<class T, class U>
inline void
StreamFace<T, U>::processErrorCode(const boost::system::error_code& error)
{
  if (error == boost::system::errc::operation_canceled) // when socket is closed by someone
    return;

  if (!m_socket->is_open())
    {
      this->fail("Connection closed");
      return;
    }

  if (error == boost::asio::error::eof)
    {
      NFD_LOG_INFO("[id:" << this->getId()
                   << ",uri:" << this->getRemoteUri()
                   << "] Connection closed");
    }
  else
    {
      NFD_LOG_WARN("[id:" << this->getId()
                   << ",uri:" << this->getRemoteUri()
                   << "] Send or receive operation failed, closing socket: "
                   << error.category().message(error.value()));
    }

  closeSocket();

  if (error == boost::asio::error::eof)
    {
      this->fail("Connection closed");
    }
  else
    {
      this->fail("Send or receive operation failed, closing socket: " +
                   error.category().message(error.value()));
    }
}


template<class T, class U>
inline void
StreamFace<T, U>::handleSend(const boost::system::error_code& error,
                             size_t nBytesSent)
{
  if (error)
    return processErrorCode(error);

  BOOST_ASSERT(!m_sendQueue.empty());

  NFD_LOG_TRACE("[id:" << this->getId()
                << ",uri:" << this->getRemoteUri()
                << "] Successfully sent: " << nBytesSent << " bytes");
  this->getMutableCounters().getNOutBytes() += nBytesSent;

  m_sendQueue.pop();
  if (!m_sendQueue.empty())
    sendFromQueue();
}

template<class T, class U>
inline void
StreamFace<T, U>::handleReceive(const boost::system::error_code& error,
                                size_t nBytesReceived)
{
  if (error)
    return processErrorCode(error);

  NFD_LOG_TRACE("[id:" << this->getId()
                << ",uri:" << this->getRemoteUri()
                << "] Received: " << nBytesReceived << " bytes");
  this->getMutableCounters().getNInBytes() += nBytesReceived;

  m_inputBufferSize += nBytesReceived;

  size_t offset = 0;

  bool isOk = true;
  Block element;
  while (m_inputBufferSize - offset > 0)
    {
      isOk = Block::fromBuffer(m_inputBuffer + offset, m_inputBufferSize - offset, element);
      if (!isOk)
        break;

      offset += element.size();

      BOOST_ASSERT(offset <= m_inputBufferSize);

      if (!this->decodeAndDispatchInput(element))
        {
          NFD_LOG_WARN("[id:" << this->getId()
                       << ",uri:" << this->getRemoteUri()
                       << "] Received unrecognized block of type ["
                       << element.type() << "]");
          // ignore unknown packet and proceed
        }
    }
  if (!isOk && m_inputBufferSize == MAX_NDN_PACKET_SIZE && offset == 0)
    {
      NFD_LOG_WARN("[id:" << this->getId()
                   << ",uri:" << this->getRemoteUri()
                   << "] Failed to parse incoming packet or it is too large to process, "
                   << "closing down the face");

      closeSocket();
      this->fail("Failed to parse incoming packet or it is too large to process, "
                   "closing down the face");
      return;
    }

  if (offset > 0)
    {
      if (offset != m_inputBufferSize)
        {
          std::copy(m_inputBuffer + offset, m_inputBuffer + m_inputBufferSize,
                    m_inputBuffer);
          m_inputBufferSize -= offset;
        }
      else
        {
          m_inputBufferSize = 0;
        }
    }

  m_socket->async_receive(boost::asio::buffer(m_inputBuffer + m_inputBufferSize,
                                              MAX_NDN_PACKET_SIZE - m_inputBufferSize), 0,
                          bind(&StreamFace<T, U>::handleReceive, this, _1, _2));
}

template<class T, class U>
inline void
StreamFace<T, U>::keepFaceAliveUntilAllHandlersExecuted(const shared_ptr<Face>& face)
{
}

template<class T, class U>
inline void
StreamFace<T, U>::closeSocket()
{
  boost::asio::io_service& io = m_socket->get_io_service();

  // use the non-throwing variants and ignore errors, if any
  boost::system::error_code error;
  m_socket->shutdown(protocol::socket::shutdown_both, error);
  m_socket->close(error);
  // after this, handlers will be called with an error code

  // ensure that the Face object is alive at least until all pending
  // handlers are dispatched
  io.post(bind(&StreamFace<T, U>::keepFaceAliveUntilAllHandlersExecuted,
               this, this->shared_from_this()));

  // clear send queue
  std::queue<Block> emptyQueue;
  std::swap(emptyQueue, m_sendQueue);
}

} // namespace nfd

#endif // NFD_DAEMON_FACE_STREAM_FACE_HPP
