#include <iostream>
#include <cmath>
#include <fstream>
#include <cstring>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

// parse the packet, return the RPM reading
float rpm(unsigned char *packet) { // 22 bytes in the packet
    return float(packet[2] | ((packet[3]<<8))) / 64.f;
}

bool verify_packet_checksum(unsigned char *packet) { // 22 bytes in the packet
    int checksum32 = 0;
    for (int i=0; i<10; i++) {
        checksum32 = (checksum32<<1) + packet[2*i] + (packet[2*i+1]<<8);
    }
    return packet[20]+(packet[21]<<8) == (((checksum32 & 0x7FFF) + (checksum32 >> 15)) & 0x7FFF);
}

int count_errors(unsigned char *buf) { // 1980 bytes in the buffer (90 packets)
    int nb_err = 0;
    for (int i=0; i<90; i++) {
        if (!verify_packet_checksum(buf+i*22)) {
            nb_err++;
        }
    }
    return nb_err;
}

// No return/max range/too low of reflectivity
bool flag1(unsigned char *data) { // 4 bytes in the data buffer
    return (data[1] & 0x80) >> 7;
}

// Object too close, possible poor reading due to proximity kicks in at < 0.6m
bool flag2(unsigned char *data) { // 4 bytes in the data buffer
    return (data[1] & 0x40) >> 6;
}

int dist_mm(unsigned char *data) { // 4 bytes in the data buffer
    return data[0] | (( data[1] & 0x3F) << 8); // 14 bits for the distance
}

int signal_strength(unsigned char *data) { // 4 bytes in the data buffer
    return data[2] | (data[3] << 8); // 16 bits for the signal strength
}

void print_all_data(unsigned char *buf) {
    int angle_degrees = 0;
    for (int p=0; p<90; p++) {
        std::cerr << "#rpm: " << rpm(buf + p*22) << std::endl;
        for (int i=0; i<4; i++) {
            unsigned char *data = buf + p*22 + 4 + i*4;
            if (flag1(data)) continue;
            std::cerr << "angle: " << angle_degrees << "\tdistance: " << dist_mm(data) << std::endl;
            angle_degrees++;
        }
    }
}

int main(int argc, char *argv[]) {
    const char default_port[] = "/dev/ttyUSB0";
    char *serial_port = (char *)default_port;
    if (2==argc) {
        serial_port = argv[1];
    }

    struct termios tty_opt;
    int tty_fd;

    std::cerr << "Opening serial port " << serial_port << std::endl;
    tty_fd = open(serial_port, O_RDWR);
    if (tty_fd < 0) {
        std::cerr << "Could not open port " << serial_port << std::endl;
        return -1;
    }

    memset(&tty_opt, 0, sizeof(tty_opt));

    tty_opt.c_cflag = CS8 | CLOCAL | CREAD; // 8N1
    tty_opt.c_iflag = 0;
    tty_opt.c_oflag = 0;
    tty_opt.c_lflag = 0;     // noncanonical mode
    tty_opt.c_cc[VMIN] = 1;  // one char is enough
    tty_opt.c_cc[VTIME] = 0; // no timer

    cfsetospeed(&tty_opt, B115200); // 115200 baud
    cfsetispeed(&tty_opt, B115200); // 115200 baud

    tcsetattr(tty_fd, TCSANOW, &tty_opt);

    unsigned char buf[1980];
    int idx = 0;
    while (1) {
        if (0==idx && 1==read(tty_fd, buf, 1) && 0xFA==buf[0]) {
            if (1==read(tty_fd, buf+1, 1) && 0xA0==buf[1]) {
                for (idx=2; idx<1980; idx++) {
                    if (1!=read(tty_fd, buf+idx, 1))
                        break;
                }
                if (0==count_errors(buf)) {
                    print_all_data(buf);
                }
                idx = 0;
            }
        }
    }
    close(tty_fd);
    return 0;
}

