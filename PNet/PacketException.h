#pragma once
#include <exception>
#include <string>

class PacketException : public std::exception
{
	const std::string m_exception;
public:
	PacketException(std::string exception) : m_exception(exception) {}
	const char* what() const noexcept override { return m_exception.c_str(); }
	std::string toString() const { return m_exception; }
};