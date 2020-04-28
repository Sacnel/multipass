/*
 * Copyright (C) 2018-2019 Canonical, Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 3.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef MULTIPASS_BACKEND_UTILS_H
#define MULTIPASS_BACKEND_UTILS_H

#include <multipass/optional.h>
#include <multipass/path.h>

#include <chrono>
#include <string>

namespace multipass
{
class MemorySize;
class ProcessFactory;

namespace backend
{
using namespace std::chrono_literals;

constexpr auto image_resize_timeout = std::chrono::duration_cast<std::chrono::milliseconds>(5min).count();

std::string generate_random_subnet();
std::string get_subnet(const Path& network_dir, const QString& bridge_name);
void resize_instance_image(const multipass::optional<MemorySize> disk_space, const multipass::Path& image_path);
Path convert_to_qcow_if_necessary(const Path& image_path);
QString cpu_arch();
void check_for_kvm_support();
void check_if_kvm_is_in_use();
} // namespace backend
} // namespace multipass
#endif // MULTIPASS_BACKEND_UTILS_H
