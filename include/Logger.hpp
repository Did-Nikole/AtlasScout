/*
 * Copyright (C) 2026 Nikole Smith (ApptsolutioNZ - appsolutionz.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#pragma once

#include <ctime>
#include <fstream>
#include <iostream>
#include <string>

class Logger {
public:
  static Logger &getInstance() {
    static Logger instance;
    return instance;
  }

  void init(const std::string &filepath, bool enabled) {
    m_enabled = enabled;
    m_filepath = filepath;
    if (m_enabled) {
      std::ofstream file(m_filepath, std::ios::app);
      if (file.is_open()) {
        file << "\n--- Logger Initialized ---\n";
      }
    }
  }

  void log(const std::string &message) {
    if (!m_enabled)
      return;
    std::ofstream file(m_filepath, std::ios::app);
    if (file.is_open()) {
      std::time_t now = std::time(nullptr);
      char buf[20];
      std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S",
                    std::localtime(&now));
      file << "[" << buf << "] " << message << "\n";
    }
  }

private:
  Logger() : m_enabled(false) {}
  ~Logger() = default;
  Logger(const Logger &) = delete;
  Logger &operator=(const Logger &) = delete;

  bool m_enabled;
  std::string m_filepath;
};
