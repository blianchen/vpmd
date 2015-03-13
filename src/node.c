/*
 * node.c
 *
 *  Created on: Mar 9, 2015
 *      Author: blc
 */

#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <arpa/inet.h>
#include <sys/time.h>

#include "epmd_int.h"

///////////////////////////////////////////////////////////
////
////////////////////////////////////////////////////////////
//#include <ifaddrs.h>
// getifaddrs 不能获得mac地址
uint64_t vpmd_uid(EpmdVars *g) {
	int fd;
	struct ifreq ifq[16];
	struct ifconf ifc;

	uint64_t rv;

	fd = socket(AF_INET, SOCK_DGRAM, 0);
	if(fd < 0) {
		dbg_tty_printf(g, 0, "Create socket error: %s(errno: %d)", strerror(errno), errno);
		return 0;
	}
	ifc.ifc_len = sizeof(ifq);
	ifc.ifc_buf = (caddr_t)ifq;
	if(ioctl(fd, SIOCGIFCONF, (char *)&ifc)) {
		dbg_tty_printf(g, 0, "ioctl SIOCGIFCONF error: %s(errno: %d)", strerror(errno), errno);
		return 0;
	}

	int num = ifc.ifc_len / sizeof(struct ifreq);
	int i;
	for(i=0; i<num; i++)
	{
		rv = 0;

		if(ioctl(fd, SIOCGIFFLAGS, (char *)&ifq[i])) {
			dbg_tty_printf(g, 0, "ioctl SIOCGIFFLAGS error: %s(errno: %d)", strerror(errno), errno);
			continue;
		}
		if ((ifq[i].ifr_flags & IFF_LOOPBACK) || !(ifq[i].ifr_flags & IFF_UP) ) {
			// is 127.0.0.1 or 接口非激活状态
			continue;
		}
//		printf("flag : %d , %d, ", ifq[i].ifr_flags & IFF_LOOPBACK, ifq[i].ifr_flags & IFF_UP);

		if(ioctl(fd, SIOCGIFHWADDR, (char *)&ifq[i])) {
			dbg_tty_printf(g, 0, "ioctl SIOCGIFHWADDR error: %s(errno: %d)", strerror(errno), errno);
			continue;
		}
		// mac, 1 byte
		rv = rv | ifq[i].ifr_hwaddr.sa_data[5];
//		rv = ( ((rv | ifq[i].ifr_hwaddr.sa_data[4]) << 8) | ifq[i].ifr_hwaddr.sa_data[5] ) << 16;
//		printf ("%X:%X:%X:%X:%X:%X \n", (unsigned char) ifq[i].ifr_hwaddr.sa_data[0], (unsigned char) ifq[i].ifr_hwaddr.sa_data[1], (unsigned char) ifq[i].ifr_hwaddr.sa_data[2], (unsigned char) ifq[i].ifr_hwaddr.sa_data[3], (unsigned char) ifq[i].ifr_hwaddr.sa_data[4], (unsigned char) ifq[i].ifr_hwaddr.sa_data[5]);

		// ip, 2 byte
		rv = rv << 16;
		if(ioctl(fd, SIOCGIFADDR, (char *)&ifq[i])) {
			dbg_tty_printf(g, 0, "ioctl SIOCGIFADDR error: %s(errno: %d)", strerror(errno), errno);
			continue;
		}
		rv = rv | ( htonl(((struct sockaddr_in*)(&ifq[i].ifr_addr))->sin_addr.s_addr) & 0xffff );
		break;
//		printf("%s", inet_ntoa(((struct sockaddr_in*)(&ifq[i].ifr_addr))->sin_addr));
	}
	close(fd);

	// time in 10 millisecond, 18 bit
	rv = rv << 18;
	struct timeval t;
	if (gettimeofday(&t, NULL) != 0) {
		dbg_tty_printf(g, 0, "gettimeofday error: %s(errno: %d)", strerror(errno), errno);
		return rv;
	}
	rv = rv | ( ((t.tv_sec * 100) + (t.tv_usec / 10000)) & 0x3ffff );

	return rv;
}

