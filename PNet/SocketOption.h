#pragma once

namespace PNet
{
	enum class SocketOption
	{
		TCP_NO_DELAY, // �� ����� TCP_NODELAY =FALSE - ����� ���� - �������� ������ �������. ��� ���������� �������� ����� ���������� � TRUE
		IPV6_ONLY, // TRUE = ������ IPv6 ����� ������������. FALSE - IPv6 � IPv4. �� ����� = TRUE
	};
}