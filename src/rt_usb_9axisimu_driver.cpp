/*
 * rt_usb_9axisimu_driver.cpp
 *
 * License: BSD-3-Clause
 *
 * Copyright (c) 2015-2020 RT Corporation <support@rt-net.jp>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of RT Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <cstring>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "rt_usb_9axisimu_driver/rt_usb_9axisimu_driver.hpp"

// Method to combine two separate one-byte data into one two-byte data
int16_t RtUsb9axisimuRosDriver::combineByteData(unsigned char data_h, unsigned char data_l)
{
  int16_t short_data = 0;

  short_data = data_h;
  short_data = short_data << 8;
  short_data |= data_l;

  return short_data;
}

// Method to extract binary sensor data from communication buffer
rt_usb_9axisimu::ImuData<int16_t>
RtUsb9axisimuRosDriver::extractBinarySensorData(unsigned char * imu_data_buf)
{
  rt_usb_9axisimu::ImuData<int16_t> imu_rawdata;

  imu_rawdata.firmware_ver = imu_data_buf[consts.IMU_BIN_FIRMWARE];
  imu_rawdata.timestamp = imu_data_buf[consts.IMU_BIN_TIMESTAMP];
  imu_rawdata.temperature =
    combineByteData(imu_data_buf[consts.IMU_BIN_TEMP_H], imu_data_buf[consts.IMU_BIN_TEMP_L]);
  imu_rawdata.ax =
    combineByteData(imu_data_buf[consts.IMU_BIN_ACC_X_H], imu_data_buf[consts.IMU_BIN_ACC_X_L]);
  imu_rawdata.ay =
    combineByteData(imu_data_buf[consts.IMU_BIN_ACC_Y_H], imu_data_buf[consts.IMU_BIN_ACC_Y_L]);
  imu_rawdata.az =
    combineByteData(imu_data_buf[consts.IMU_BIN_ACC_Z_H], imu_data_buf[consts.IMU_BIN_ACC_Z_L]);
  imu_rawdata.gx =
    combineByteData(imu_data_buf[consts.IMU_BIN_GYRO_X_H], imu_data_buf[consts.IMU_BIN_GYRO_X_L]);
  imu_rawdata.gy =
    combineByteData(imu_data_buf[consts.IMU_BIN_GYRO_Y_H], imu_data_buf[consts.IMU_BIN_GYRO_Y_L]);
  imu_rawdata.gz =
    combineByteData(imu_data_buf[consts.IMU_BIN_GYRO_Z_H], imu_data_buf[consts.IMU_BIN_GYRO_Z_L]);
  imu_rawdata.mx =
    combineByteData(imu_data_buf[consts.IMU_BIN_MAG_X_H], imu_data_buf[consts.IMU_BIN_MAG_X_L]);
  imu_rawdata.my =
    combineByteData(imu_data_buf[consts.IMU_BIN_MAG_Y_H], imu_data_buf[consts.IMU_BIN_MAG_Y_L]);
  imu_rawdata.mz =
    combineByteData(imu_data_buf[consts.IMU_BIN_MAG_Z_H], imu_data_buf[consts.IMU_BIN_MAG_Z_L]);

  return imu_rawdata;
}

bool RtUsb9axisimuRosDriver::isBinarySensorData(unsigned char * imu_data_buf)
{
  if (imu_data_buf[consts.IMU_BIN_HEADER_R] == 'R' &&
    imu_data_buf[consts.IMU_BIN_HEADER_T] == 'T')
  {
    return true;
  }
  return false;
}

bool RtUsb9axisimuRosDriver::readBinaryData(void)
{
  static std::vector<unsigned char> imu_binary_data_buffer;
  unsigned char read_data_buf[256];

  has_refreshed_imu_data_ = false;
  int read_data_size = serial_port_->readFromDevice(read_data_buf,
    consts.IMU_BIN_DATA_SIZE - imu_binary_data_buffer.size());

  if(read_data_size == 0){  // The device was unplugged.
    return false;
  }

  if(read_data_size < 0){  // read() returns error code.
    if(errno == EAGAIN || errno == EWOULDBLOCK){  // Wainting for data.
      return true;
    }else{
      return false;
    }
  }

  for(int i = 0; i < read_data_size; i++){
    imu_binary_data_buffer.push_back(read_data_buf[i]);
  }

  if (imu_binary_data_buffer.size() < consts.IMU_BIN_DATA_SIZE){
    return true;
  }

  if (isBinarySensorData(imu_binary_data_buffer.data()) == false) {
    imu_binary_data_buffer.clear();
    return false;
  }

  auto imu_rawdata = extractBinarySensorData(imu_binary_data_buffer.data());
  imu_binary_data_buffer.clear();

  sensor_data_.setImuRawData(imu_rawdata);  // Update raw data
  sensor_data_.convertRawDataUnit();        // Convert raw data to physical quantity
  has_refreshed_imu_data_ = true;

  return true;
}

bool RtUsb9axisimuRosDriver::isValidAsciiSensorData(std::vector<std::string> str_vector)
{
  for (int i = 1; i < consts.IMU_ASCII_DATA_SIZE; i++) {
    if (strspn(str_vector[i].c_str(), "-.0123456789") != str_vector[i].size()) {
      return false;
    }
  }
  return true;
}

bool RtUsb9axisimuRosDriver::readAsciiData(void)
{
  static std::vector<std::string> imu_data_vector_buf;

  unsigned char imu_data_buf[256];
  rt_usb_9axisimu::ImuData<double> imu_data;
  std::string imu_data_oneline_buf;

  has_refreshed_imu_data_ = false;
  imu_data_oneline_buf.clear();

  int data_size_of_buf = serial_port_->readFromDevice(imu_data_buf, sizeof(imu_data_buf));

  if (data_size_of_buf <= 0) {
    return false;  // Raise communication error
  }

  for (int char_count = 0; char_count < data_size_of_buf; char_count++) {
    if (imu_data_buf[char_count] == ',' || imu_data_buf[char_count] == '\n') {
      imu_data_vector_buf.push_back(imu_data_oneline_buf);
      // If the imu_data_oneline_buf is empty string (such as receiving
      // ',' and '\n' continuously), clear the imu_data_vector_buf.
      if (imu_data_oneline_buf.empty()) {
        imu_data_vector_buf.clear();
      }
      imu_data_oneline_buf.clear();
    } else {
      imu_data_oneline_buf += imu_data_buf[char_count];
    }

    if (imu_data_buf[char_count] == '\n' &&
      imu_data_vector_buf.size() == consts.IMU_ASCII_DATA_SIZE &&
      imu_data_vector_buf[0].find(".") == std::string::npos &&
      isValidAsciiSensorData(imu_data_vector_buf))
    {
      imu_data.gx = std::stof(imu_data_vector_buf[consts.IMU_ASCII_GYRO_X]);
      imu_data.gy = std::stof(imu_data_vector_buf[consts.IMU_ASCII_GYRO_Y]);
      imu_data.gz = std::stof(imu_data_vector_buf[consts.IMU_ASCII_GYRO_Z]);
      imu_data.ax = std::stof(imu_data_vector_buf[consts.IMU_ASCII_ACC_X]);
      imu_data.ay = std::stof(imu_data_vector_buf[consts.IMU_ASCII_ACC_Y]);
      imu_data.az = std::stof(imu_data_vector_buf[consts.IMU_ASCII_ACC_Z]);
      imu_data.mx = std::stof(imu_data_vector_buf[consts.IMU_ASCII_MAG_X]);
      imu_data.my = std::stof(imu_data_vector_buf[consts.IMU_ASCII_MAG_Y]);
      imu_data.mz = std::stof(imu_data_vector_buf[consts.IMU_ASCII_MAG_Z]);
      imu_data.temperature = std::stof(imu_data_vector_buf[consts.IMU_ASCII_TEMP]);

      imu_data_vector_buf.clear();
      sensor_data_.setImuData(imu_data);
      has_refreshed_imu_data_ = true;
    } else if (imu_data_vector_buf.size() > consts.IMU_ASCII_DATA_SIZE) {
      imu_data_vector_buf.clear();
    }
  }

  return true;
}

RtUsb9axisimuRosDriver::RtUsb9axisimuRosDriver(std::string port = "")
{
  serial_port_ = std::make_unique<rt_usb_9axisimu::SerialPort>(port.c_str());
  has_completed_format_check_ = false;
  data_format_ = DataFormat::NONE;
  has_refreshed_imu_data_ = false;
}

RtUsb9axisimuRosDriver::RtUsb9axisimuRosDriver(std::unique_ptr<rt_usb_9axisimu::SerialPort> serial_port)
{
  serial_port_ = std::move(serial_port);
  has_completed_format_check_ = false;
  data_format_ = DataFormat::NONE;
  has_refreshed_imu_data_ = false;
}

RtUsb9axisimuRosDriver::~RtUsb9axisimuRosDriver()
{
}

void RtUsb9axisimuRosDriver::setImuFrameIdName(std::string frame_id)
{
  frame_id_ = frame_id;
}

void RtUsb9axisimuRosDriver::setImuPortName(std::string port)
{
  serial_port_->setPort(port.c_str());
}

void RtUsb9axisimuRosDriver::setImuStdDev(
  double linear_acceleration, double angular_velocity,
  double magnetic_field)
{
  linear_acceleration_stddev_ = linear_acceleration;
  angular_velocity_stddev_ = angular_velocity;
  magnetic_field_stddev_ = magnetic_field;
}

bool RtUsb9axisimuRosDriver::startCommunication()
{
  // returns serial port open status
  return serial_port_->openSerialPort();
}

void RtUsb9axisimuRosDriver::stopCommunication(void)
{
  serial_port_->closeSerialPort();
  has_completed_format_check_ = false;
  data_format_ = DataFormat::NONE;
  has_refreshed_imu_data_ = false;
}

void RtUsb9axisimuRosDriver::checkDataFormat(void)
{
  if (data_format_ == DataFormat::NONE) {
    unsigned char data_buf[256];
    int data_size_of_buf = serial_port_->readFromDevice(data_buf, consts.IMU_BIN_DATA_SIZE);
    if (data_size_of_buf == consts.IMU_BIN_DATA_SIZE) {
      if (isBinarySensorData(data_buf)) {
        data_format_ = DataFormat::BINARY;
        has_completed_format_check_ = true;
      } else {
        data_format_ = DataFormat::NOT_BINARY;
      }
    }
  } else if (data_format_ == DataFormat::NOT_BINARY) {
    data_format_ = DataFormat::ASCII;
    has_completed_format_check_ = true;
  }
}

bool RtUsb9axisimuRosDriver::hasCompletedFormatCheck(void)
{
  return has_completed_format_check_;
}

bool RtUsb9axisimuRosDriver::hasAsciiDataFormat(void)
{
  return data_format_ == DataFormat::ASCII;
}

bool RtUsb9axisimuRosDriver::hasBinaryDataFormat(void)
{
  return data_format_ == DataFormat::BINARY;
}

bool RtUsb9axisimuRosDriver::hasRefreshedImuData(void)
{
  return has_refreshed_imu_data_;
}

std::unique_ptr<sensor_msgs::msg::Imu> RtUsb9axisimuRosDriver::getImuRawDataUniquePtr(
  const rclcpp::Time timestamp)
{
  auto imu = sensor_data_.getImuData();  // Get physical quantity
  auto imu_data_raw_msg = std::make_unique<sensor_msgs::msg::Imu>();

  // Calculate linear_acceleration_covariance diagonal elements
  double linear_acceleration_cov = linear_acceleration_stddev_ * linear_acceleration_stddev_;
  // Calculate angular_velocity_covariance diagonal elements
  double angular_velocity_cov = angular_velocity_stddev_ * angular_velocity_stddev_;

  // imu_data_raw_msg has no orientation values
  imu_data_raw_msg->orientation_covariance[0] = -1;

  imu_data_raw_msg->linear_acceleration_covariance[0] =
    imu_data_raw_msg->linear_acceleration_covariance[4] =
    imu_data_raw_msg->linear_acceleration_covariance[8] = linear_acceleration_cov;

  imu_data_raw_msg->angular_velocity_covariance[0] =
    imu_data_raw_msg->angular_velocity_covariance[4] =
    imu_data_raw_msg->angular_velocity_covariance[8] = angular_velocity_cov;

  imu_data_raw_msg->header.stamp = timestamp;
  imu_data_raw_msg->header.frame_id = frame_id_;

  // original data used the g unit, convert to m/s^2
  imu_data_raw_msg->linear_acceleration.x = imu.ax * consts.CONVERTOR_G2A;
  imu_data_raw_msg->linear_acceleration.y = imu.ay * consts.CONVERTOR_G2A;
  imu_data_raw_msg->linear_acceleration.z = imu.az * consts.CONVERTOR_G2A;

  if (data_format_ == DataFormat::BINARY) {
    // original binary data used the degree/s unit, convert to radian/s
    imu_data_raw_msg->angular_velocity.x = imu.gx * consts.CONVERTOR_D2R;
    imu_data_raw_msg->angular_velocity.y = imu.gy * consts.CONVERTOR_D2R;
    imu_data_raw_msg->angular_velocity.z = imu.gz * consts.CONVERTOR_D2R;
  } else if (data_format_ == DataFormat::ASCII) {
    // original ascii data used the radian/s
    imu_data_raw_msg->angular_velocity.x = imu.gx;
    imu_data_raw_msg->angular_velocity.y = imu.gy;
    imu_data_raw_msg->angular_velocity.z = imu.gz;
  }

  return imu_data_raw_msg;
}

std::unique_ptr<sensor_msgs::msg::MagneticField> RtUsb9axisimuRosDriver::getImuMagUniquePtr(
  const rclcpp::Time timestamp)
{
  auto imu = sensor_data_.getImuData();  // Get physical quantity
  auto imu_magnetic_msg = std::make_unique<sensor_msgs::msg::MagneticField>();

  double magnetic_field_cov = magnetic_field_stddev_ * magnetic_field_stddev_;

  imu_magnetic_msg->magnetic_field_covariance[0] = imu_magnetic_msg->magnetic_field_covariance[4] =
    imu_magnetic_msg->magnetic_field_covariance[8] = magnetic_field_cov;

  imu_magnetic_msg->header.stamp = timestamp;
  imu_magnetic_msg->header.frame_id = frame_id_;

  // original data used the uTesla unit, convert to Tesla
  imu_magnetic_msg->magnetic_field.x = imu.mx / consts.CONVERTOR_UT2T;
  imu_magnetic_msg->magnetic_field.y = imu.my / consts.CONVERTOR_UT2T;
  imu_magnetic_msg->magnetic_field.z = imu.mz / consts.CONVERTOR_UT2T;

  return imu_magnetic_msg;
}

std::unique_ptr<std_msgs::msg::Float64> RtUsb9axisimuRosDriver::getImuTemperatureUniquePtr(void)
{
  auto imu = sensor_data_.getImuData();  // Get physical quantity
  auto imu_temperature_msg = std::make_unique<std_msgs::msg::Float64>();

  // original data used the celsius unit
  imu_temperature_msg->data = imu.temperature;

  return imu_temperature_msg;
}

// Method to receive IMU data, convert those units to SI, and publish to ROS
// topic
bool RtUsb9axisimuRosDriver::readSensorData()
{
  if (data_format_ == DataFormat::BINARY) {
    return readBinaryData();
  } else if (data_format_ == DataFormat::ASCII) {
    return readAsciiData();
  }

  return false;
}
