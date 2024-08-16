//
// Created by User on 30/07/2024.
//

#ifndef FILELOGGERSTREAM_H
#define FILELOGGERSTREAM_H

#include <fstream>
#include <boost/asio/ssl.hpp>

class FileLoggerStream : public std::ostream {
public:
    explicit FileLoggerStream(const std::string &filename)
        : std::ostream(nullptr), m_file(filename, std::ios_base::app) {
        if (!m_file.is_open()) {
            throw std::runtime_error("Unable to open log file: " + filename);
        }
        rdbuf(m_file.rdbuf());
    }

    ~FileLoggerStream() override {
        close();
    }

    void close() {
        if (m_file.is_open()) {
            m_file.close();
        }
    }

private:
    std::ofstream m_file;
};

#endif //FILELOGGERSTREAM_H
