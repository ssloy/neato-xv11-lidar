#include <iostream>
#include <cstring>
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>

float rpm(unsigned char *packet) { // 22 bytes in the packet
    return float(packet[2] | ((packet[3]<<8))) / 64.f;
}

bool verify_packet_checksum(unsigned char *packet) { // 22 bytes in the packet
    int checksum32 = 0;
    for (int i=0; i<10; i++)
        checksum32 = (checksum32<<1) + packet[2*i] + (packet[2*i+1]<<8);
    return packet[20]+(packet[21]<<8) == (((checksum32 & 0x7FFF) + (checksum32 >> 15)) & 0x7FFF);
}

int count_errors(unsigned char *buf) { // 1980 bytes in the buffer (90 packets)
    int nb_err = 0;
    for (int i=0; i<90; i++) {
        nb_err += !verify_packet_checksum(buf+i*22);
    }
    return nb_err;
}

// no return/max range/too low of reflectivity
bool invalid_data_flag(unsigned char *data) { // 4 bytes in the data buffer
    return (data[1] & 0x80) >> 7;
}

// object too close, possible poor reading due to proximity; kicks in at < 0.6m
bool strength_warning_flag(unsigned char *data) { // 4 bytes in the data buffer
    return (data[1] & 0x40) >> 6;
}

int dist_mm(unsigned char *data) { // 4 bytes in the data buffer
    return data[0] | (( data[1] & 0x3F) << 8); // 14 bits for the distance
}

int signal_strength(unsigned char *data) { // 4 bytes in the data buffer
    return data[2] | (data[3] << 8); // 16 bits for the signal strength
}

void print_all_data(unsigned char *buf) {
    for (int p=0; p<90; p++) { // for all 90 packets
        std::cerr << "#rpm: " << rpm(buf + p*22) << std::endl;
        for (int i=0; i<4; i++) { // process 4 chunks per packet
            unsigned char *data = buf + p*22 + 4 + i*4; // current chunk pointer
            if (!invalid_data_flag(data)) {
                int angle_degrees = p*4+i;
                std::cerr << "angle: " << angle_degrees << "\tdistance: " << dist_mm(data) << std::endl;
            }
        }
    }
}

void init_serial_port(int &tty_fd) { // 115200 baud 8n1 blocking read
    struct termios tty_opt;
    memset(&tty_opt, 0, sizeof(tty_opt));
    tty_opt.c_cflag = CS8 | CLOCAL | CREAD; // CS8: 8n1, CLOCAL: local connection, no modem contol, CREAD: enable receiving characters
    tty_opt.c_iflag = 0;
    tty_opt.c_oflag = 0;
    tty_opt.c_lflag = 0;     // non-canonical mode
    tty_opt.c_cc[VMIN] = 1;  // blocking read until 1 character arrives
    tty_opt.c_cc[VTIME] = 0; // inter-character timer unused
    cfsetospeed(&tty_opt, B115200);
    cfsetispeed(&tty_opt, B115200);
    tcsetattr(tty_fd, TCSANOW, &tty_opt);
}

int main(int argc, char *argv[]) {
    const char default_port[] = "/dev/ttyUSB0";
    char *serial_port = (char *)default_port;
    if (2==argc) {
        serial_port = argv[1];
    }

    std::cerr << "Opening serial port " << serial_port << std::endl;
    int tty_fd = open(serial_port, O_RDWR);
    if (tty_fd < 0) {
        std::cerr << "Could not open port " << serial_port << std::endl;
        return -1;
    }
    init_serial_port(tty_fd);

    unsigned char buf[1980];
    while (1) {
        if (1==read(tty_fd, buf, 1) && 0xFA==buf[0] && 1==read(tty_fd, buf+1, 1) && 0xA0==buf[1]) { // find the header 0xFA 0xA0
            for (int idx=2; idx<1980; idx++) // register all the 360 readings (90 packets, 22 bytesh each)
                if (1!=read(tty_fd, buf+idx, 1)) break;
            if (!count_errors(buf)) { // if no errors during the transmission
                print_all_data(buf);  // then print the data to the screen
            }
        }
    }

    close(tty_fd);
    return 0;
}

