#include "io.hpp"

#include <map>
#include <stdint.h>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>

#include "coders/commons.hpp"
#include "coders/gzip.hpp"
#include "coders/json.hpp"
#include "coders/toml.hpp"
#include "util/stringutil.hpp"

#include "devices/Device.hpp"

namespace fs = std::filesystem;

static std::map<std::string, std::shared_ptr<io::Device>> devices;

void io::set_device(const std::string& name, std::shared_ptr<io::Device> device) {
    devices[name] = device;
}

void io::remove_device(const std::string& name) {
    devices.erase(name);
}

std::shared_ptr<io::Device> io::get_device(const std::string& name) {
    const auto& found = devices.find(name);
    if (found == devices.end()) {
        return nullptr;
    }
    return found->second;
}

io::Device& io::require_device(const std::string& name) {
    auto device = get_device(name);
    if (!device) {
        throw std::runtime_error("io-device not found: " + name);
    }
    return *device;
}

void io::create_subdevice(
    const std::string& name, const std::string& parent, const io::path& root
) {
    auto parentDevice = get_device(parent);
    if (!parentDevice) {
        throw std::runtime_error("parent device not found for entry-point: " + parent);
    }
    set_device(name, std::make_shared<io::SubDevice>(parentDevice, root.pathPart()));
}

io::directory_iterator::directory_iterator(const io::path& folder)
    : folder(folder) {
    auto& device = io::require_device(folder.entryPoint());
    generator = device.list(folder.pathPart());
}

io::rafile::rafile(const io::path& filename)
    : file(io::resolve(filename), std::ios::binary | std::ios::ate) {
    if (!file) {
        throw std::runtime_error("could not to open file " + filename.string());
    }
    filelength = file.tellg();
    file.seekg(0);
}

size_t io::rafile::length() const {
    return filelength;
}

void io::rafile::seekg(std::streampos pos) {
    file.seekg(pos);
}

void io::rafile::read(char* buffer, std::streamsize size) {
    file.read(buffer, size);
}

bool io::write_bytes(
    const io::path& filename, const ubyte* data, size_t size
) {
    auto device = io::get_device(filename.entryPoint());
    if (device == nullptr) {
        return false;
    }
    device->write(filename.pathPart(), data, size);
    return true;
}

bool io::read(const io::path& filename, char* data, size_t size) {
    auto device = io::get_device(filename.entryPoint());
    if (device == nullptr) {
        return false;
    }
    device->read(filename.pathPart(), data, size);
    return true;
}

util::Buffer<ubyte> io::read_bytes_buffer(const path& file) {
    size_t size;
    auto bytes = io::read_bytes(file, size);
    return util::Buffer<ubyte>(std::move(bytes), size);
}

std::unique_ptr<ubyte[]> io::read_bytes(
    const io::path& filename, size_t& length
) {
    auto& device = io::require_device(filename.entryPoint());
    length = device.size(filename.pathPart());
    auto data = std::make_unique<ubyte[]>(length);
    device.read(filename.pathPart(), data.get(), length);
    return data;
}

std::vector<ubyte> io::read_bytes(const path& filename) {
    auto& device = io::require_device(filename.entryPoint());
    size_t length = device.size(filename.pathPart());
    std::vector<ubyte> data(length);
    device.read(filename.pathPart(), data.data(), length);
    return data;
}

std::string io::read_string(const path& filename) {
    size_t size;
    auto bytes = read_bytes(filename, size);
    return std::string((const char*)bytes.get(), size);
}

bool io::write_string(const io::path& file, std::string_view content) {
    return io::write_bytes(file, (const ubyte*)content.data(), content.size());
}

bool io::write_json(
    const io::path& file, const dv::value& obj, bool nice
) {
    return io::write_string(file, json::stringify(obj, nice, "  "));
}

bool io::write_binary_json(
    const io::path& file, const dv::value& obj, bool compression
) {
    auto bytes = json::to_binary(obj, compression);
    return io::write_bytes(file, bytes.data(), bytes.size());
}

dv::value io::read_json(const path& filename) {
    std::string text = io::read_string(filename);
    return json::parse(filename.string(), text);
}

dv::value io::read_binary_json(const path& file) {
    size_t size;
    auto bytes = io::read_bytes(file, size);
    return json::from_binary(bytes.get(), size);
}

dv::value io::read_toml(const path& file) {
    return toml::parse(file.string(), io::read_string(file));
}

std::vector<std::string> io::read_list(const io::path& filename) {
    std::ifstream file(resolve(filename)); // FIXME
    if (!file) {
        throw std::runtime_error(
            "could not to open file " + filename.string()
        );
    }
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(file, line)) {
        util::trim(line);
        if (line.length() == 0) continue;
        if (line[0] == '#') continue;
        lines.push_back(line);
    }
    return lines;
}

bool io::is_regular_file(const io::path& file) {
    if (file.emptyOrInvalid()) {
        return false;
    }
    auto device = io::get_device(file.entryPoint());
    if (device == nullptr) {
        return false;
    }
    return device->isfile(file.pathPart());
}

bool io::is_directory(const io::path& file) {
    if (file.emptyOrInvalid()) {
        return false;
    }
    auto device = io::get_device(file.entryPoint());
    if (device == nullptr) {
        return false;
    }
    return device->isdir(file.pathPart());
}

bool io::exists(const io::path& file) {
    if (file.emptyOrInvalid()) {
        return false;
    }
    auto device = io::get_device(file.entryPoint());
    if (device == nullptr) {
        return false;
    }
    return device->exists(file.pathPart());
}

bool io::create_directories(const io::path& file) {
    auto& device = io::require_device(file.entryPoint());
    if (device.isdir(file.pathPart())) {
        return false;
    }
    device.mkdirs(file.pathPart());
    return true;
}

bool io::remove(const io::path& file) {
    auto& device = io::require_device(file.entryPoint());
    return device.remove(file.pathPart());
}

uint64_t io::remove_all(const io::path& file) {
    auto& device = io::require_device(file.entryPoint());
    return device.removeAll(file.pathPart());
}

size_t io::file_size(const io::path& file) {
    auto& device = io::require_device(file.entryPoint());
    return device.size(file.pathPart());
}

std::filesystem::path io::resolve(const io::path& file) {
    auto device = io::get_device(file.entryPoint());
    if (device == nullptr) {
        return {};
    }
    return device->resolve(file.pathPart());
}

#include <map>

#include "coders/json.hpp"
#include "coders/toml.hpp"

using DecodeFunc = dv::value(*)(std::string_view, std::string_view);

static std::map<fs::path, DecodeFunc> data_decoders {
    {fs::u8path(".json"), json::parse},
    {fs::u8path(".toml"), toml::parse},
};

bool io::is_data_file(const io::path& file) {
    return is_data_interchange_format(file.extension());
}

bool io::is_data_interchange_format(const std::string& ext) {
    return data_decoders.find(ext) != data_decoders.end();
}

dv::value io::read_object(const path& file) {
    const auto& found = data_decoders.find(file.extension());
    if (found == data_decoders.end()) {
        throw std::runtime_error("unknown file format");
    }
    auto text = read_string(file);
    try {
        return found->second(file.string(), text);
    } catch (const parsing_error& err) {
        throw std::runtime_error(err.errorLog());
    }
}
