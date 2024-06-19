#include "paker.hpp"
#include "nlohmann/json.hpp"
#include "zlib.h"
#include <climits>
#include <cmath>
#include <format>
#include <fstream>

namespace pl {
using json = nlohmann::json;

const char plpaker_version[] = "0.12.0";

const char compressed_extension[] = ".comp";
const char pakinfo_json[] = "pakinfo.json";

struct scope_data {
    explicit scope_data(size_t size) {
        ptr = new char[size];
    }

    ~scope_data() {
        delete[] ptr;
    }

    char* ptr = nullptr;
};

uint32_t to_uint_be(char* c) {
    return uint32_t((unsigned char)(c[0]) << 24
                    | (unsigned char)(c[1]) << 16
                    | (unsigned char)(c[2]) << 8
                    | (unsigned char)(c[3]));
}

uint32_t to_uint_le(char* c) {
    return uint32_t((unsigned char)(c[3]) << 24
                    | (unsigned char)(c[2]) << 16
                    | (unsigned char)(c[1]) << 8
                    | (unsigned char)(c[0]));
}

// https://stackoverflow.com/questions/105252/how-do-i-convert-between-big-endian-and-little-endian-values-in-c
template <typename T>
T swap_endian(T u) {
    static_assert(CHAR_BIT == 8, "CHAR_BIT != 8");

    union {
        T u;
        unsigned char u8[sizeof(T)];
    } source, dest;

    source.u = u;

    for (size_t k = 0; k < sizeof(T); k++)
        dest.u8[k] = source.u8[sizeof(T) - k - 1];

    return dest.u;
}

//-----------------------------------------------------------------------------
bool pak::parse(fs::path const& pak_file) {
    std::ifstream file(pak_file, std::ios::binary);
    if (!file)
        return false;

    file.seekg(0, file.end);
    length = file.tellg();
    file.seekg(0, file.beg);

    crc_pos = length - sizeof(index_begin) - sizeof(crc_value);
    file.seekg(crc_pos);
    file.read((char*)&crc_value, sizeof(crc_value));

    index_pos = length - sizeof(index_begin);
    file.seekg(index_pos);
    file.read((char*)&index_begin, sizeof(index_begin));

    file.seekg(index_begin);
    file.read((char*)&version, sizeof(version));
    file.read((char*)&count, sizeof(count));

    for (auto index = 0u; index < count; ++index) {
        pak::item item;
        item.index = index;
        item.pos = file.tellg();

        file.read((char*)&item.compressed, sizeof(item.compressed));

        char str_length_bytes[4];
        file.read(str_length_bytes, sizeof(str_length_bytes));
        auto str_length = to_uint_be(str_length_bytes);

        {
            int64_t const filename_pos = file.tellg();

            char filename[256];
            file.read(filename, str_length);

            if (filename[0] == 1) { // 128+ offset
                file.seekg(filename_pos + 1);
                file.read(filename, str_length);
            }

            filename[str_length] = '\0';
            item.filename = filename;
        }

        file.read((char*)&item.begin, sizeof(item.begin));
        file.read((char*)&item.size, sizeof(item.size));

        max_size = std::max(max_size, item.size);

        if (index != 0) // skip first
            items.at(index - 1).end = item.begin;

        items.push_back(std::move(item));
    };

    // last before index
    items.at(count - 1).end = index_begin;

    file.close();
    return true;
}

//-----------------------------------------------------------------------------
bool pak::load(fs::path const& pakinfo_file) {
    std::ifstream file(pakinfo_file, std::ios::binary);
    if (!file)
        return false;

    file.seekg(0, file.end);
    auto const data_size = file.tellg();
    file.seekg(0, file.beg);

    if (data_size == 0)
        return false;

    scope_data data(data_size);
    file.read(data.ptr, data_size);

    if (!json::accept(data.ptr, data.ptr + data_size))
        return false;

    auto j = json::parse(data.ptr, data.ptr + data_size);
    if (j.count("paker")) {
        string const j_version = j["paker"];
        if (j_version != plpaker_version)
            return false;
    }

    if (j.count("crc_pos"))
        crc_pos = j["crc_pos"];
    if (j.count("crc_value"))
        crc_value = j["crc_value"];
    if (j.count("index_pos"))
        index_pos = j["index_pos"];
    if (j.count("index_begin"))
        index_begin = j["index_begin"];
    if (j.count("version"))
        version = j["version"];
    if (j.count("count"))
        count = j["count"];

    if (j.count("files")) {
        auto j_files = j["files"];
        for (auto& j_item : j_files) {
            pak::item item;

            if (j_item.count("index"))
                item.index = j_item["index"];
            if (j_item.count("pos"))
                item.pos = j_item["pos"];
            if (j_item.count("compressed"))
                item.compressed = j_item["compressed"];
            if (j_item.count("filename"))
                item.filename = j_item["filename"];
            if (j_item.count("begin"))
                item.begin = j_item["begin"];
            if (j_item.count("end"))
                item.end = j_item["end"];
            if (j_item.count("size"))
                item.size = j_item["size"];
            if (j_item.count("size_compressed"))
                item.size_compressed = j_item["size_compressed"];

            items.push_back(std::move(item));
        }
    }

    if (j.count("length"))
        length = j["length"];
    if (j.count("max_size"))
        max_size = j["max_size"];

    return true;
}

//-----------------------------------------------------------------------------
bool pak::write_info(fs::path const& output_path) const {
    auto json_file = output_path;
    json_file += fs::path::preferred_separator;
    json_file += pakinfo_json;

    if (fs::exists(json_file))
        fs::remove(json_file);

    std::ofstream file(json_file);
    if (!file)
        return false;

    auto j_files = json::array();
    for (auto const& item : items) {
        json j_file;
        j_file["index"] = item.index;
        j_file["pos"] = item.pos;
        j_file["compressed"] = item.compressed;
        j_file["filename"] = item.filename;
        j_file["begin"] = item.begin;
        j_file["end"] = item.end;
        j_file["size"] = item.size;

        if (item.size_compressed > 0)
            j_file["size_compressed"] = item.size_compressed;

        j_files.push_back(j_file);
    }

    json j;
    j["paker"] = plpaker_version;

    j["length"] = length;
    j["max_size"] = max_size;

    j["crc_pos"] = crc_pos;
    j["crc_value"] = crc_value;
    j["index_pos"] = index_pos;
    j["index_begin"] = index_begin;
    j["version"] = version;

    j["count"] = count;
    j["files"] = j_files;

    auto const j_string = j.dump(4);
    file.write(j_string.data(), j_string.size());

    return true;
}

//-----------------------------------------------------------------------------
bool compress_data(char* const decompressed_data, size_t& decompressed_data_size,
                   char* compressed_data, size_t& compressed_data_size) {
    z_stream stream = {0};
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;

    auto res = deflateInit2(&stream,
                            Z_DEFAULT_COMPRESSION,
                            Z_DEFLATED,
                            31,
                            8,
                            Z_DEFAULT_STRATEGY);
    if (res < 0)
        return false;

    stream.avail_in = decompressed_data_size;
    stream.next_in = reinterpret_cast<unsigned char*>(decompressed_data);

    stream.avail_out = compressed_data_size;
    stream.next_out = reinterpret_cast<unsigned char*>(compressed_data);

    res = deflate(&stream, Z_FINISH);
    if (res < 0)
        return false;

    compressed_data_size = stream.total_out;

    res = deflateEnd(&stream);
    if (res < 0)
        return false;

    return true;
}

//-----------------------------------------------------------------------------
bool compress_file(fs::path const& input_file,
                   fs::path const& output_file) {
    std::ifstream decompressed_file(input_file, std::ios::binary);
    if (!decompressed_file)
        return false;

    decompressed_file.seekg(0, decompressed_file.end);
    size_t decompressed_size = decompressed_file.tellg();
    decompressed_file.seekg(0, decompressed_file.beg);

    int64_t const max_data_size = std::pow(2, std::ceil(std::log2(decompressed_size)));

    scope_data decompressed_data(max_data_size);

    decompressed_file.read(decompressed_data.ptr, decompressed_size);
    decompressed_file.close();

    scope_data compressed_data(max_data_size);
    size_t compressed_data_size = max_data_size;

    if (!compress_data(decompressed_data.ptr,
                       decompressed_size,
                       compressed_data.ptr,
                       compressed_data_size))
        return false;

    if (fs::exists(output_file))
        fs::remove(output_file);

    std::ofstream compressed_file(output_file, std::ios::binary);
    if (!compressed_file)
        return false;

    compressed_file.write(compressed_data.ptr, compressed_data_size);

    compressed_file.close();
    return true;
}

//-----------------------------------------------------------------------------
bool decompress_data(char* const compressed_data, size_t& compressed_data_size,
                     char* decompressed_data, size_t& decompressed_data_size) {
    z_stream stream = {0};
    stream.zalloc = Z_NULL;
    stream.zfree = Z_NULL;
    stream.opaque = Z_NULL;
    auto res = inflateInit2(&stream, 31);
    if (res < 0)
        return false;

    stream.avail_in = compressed_data_size;
    stream.next_in = reinterpret_cast<unsigned char*>(compressed_data);

    stream.avail_out = decompressed_data_size;
    stream.next_out = reinterpret_cast<unsigned char*>(decompressed_data);

    res = inflate(&stream, Z_NO_FLUSH);
    if (res < 0)
        return false;

    decompressed_data_size = stream.total_out;

    res = inflateEnd(&stream);
    if (res < 0)
        return false;

    return true;
}

//-----------------------------------------------------------------------------
bool decompress_file(fs::path const& input_file,
                     fs::path const& output_file) {
    std::ifstream compressed_file(input_file, std::ios::binary);
    if (!compressed_file)
        return false;

    compressed_file.seekg(0, compressed_file.end);
    size_t const compressed_size = compressed_file.tellg();
    compressed_file.seekg(0, compressed_file.beg);

    scope_data compressed_data(compressed_size);

    compressed_file.read(compressed_data.ptr, compressed_size);
    compressed_file.close();

    size_t compressed_data_size = compressed_size;

    int64_t const max_data_size = std::pow(2, std::ceil(std::log2(compressed_data_size * 2)));

    scope_data data_decompressed(max_data_size);
    size_t decompressed_data_size = max_data_size;

    if (!decompress_data(compressed_data.ptr,
                         compressed_data_size,
                         data_decompressed.ptr,
                         decompressed_data_size))
        return false;

    if (fs::exists(output_file))
        fs::remove(output_file);

    std::ofstream decompressed_file(output_file, std::ios::binary);
    if (!decompressed_file)
        return false;

    decompressed_file.write(data_decompressed.ptr, decompressed_data_size);

    decompressed_file.close();
    return true;
}

//-----------------------------------------------------------------------------
void write_index(pak::ptr pak, std::ofstream& stream, uLong crc) {
    int64_t const index_begin = stream.tellp();
    stream.write(reinterpret_cast<const char*>(&pak->version), sizeof(pak->version));
    crc = crc32(crc, reinterpret_cast<const Bytef*>(&pak->version), sizeof(pak->version));
    stream.write(reinterpret_cast<const char*>(&pak->count), sizeof(pak->count));
    crc = crc32(crc, reinterpret_cast<const Bytef*>(&pak->count), sizeof(pak->count));

    for (auto const& item : pak->items) {

        stream.write(reinterpret_cast<const char*>(&item.compressed), sizeof(item.compressed));
        crc = crc32(crc, reinterpret_cast<const Bytef*>(&item.compressed), sizeof(item.compressed));

        auto const name_size = swap_endian<int32_t>(item.filename.size());
        stream.write(reinterpret_cast<const char*>(&name_size), sizeof(name_size));
        crc = crc32(crc, reinterpret_cast<const Bytef*>(&name_size), sizeof(name_size));

        if (item.filename.size() >= 128) {
            bool const offset = true;
            stream.write(reinterpret_cast<const char*>(&offset), sizeof(offset));
            crc = crc32(crc, reinterpret_cast<const Bytef*>(&offset), sizeof(offset));
        }

        stream.write(item.filename.c_str(), item.filename.size());
        crc = crc32(crc, reinterpret_cast<const Bytef*>(item.filename.c_str()), item.filename.size());

        stream.write(reinterpret_cast<const char*>(&item.begin), sizeof(item.begin));
        crc = crc32(crc, reinterpret_cast<const Bytef*>(&item.begin), sizeof(item.begin));
        stream.write(reinterpret_cast<const char*>(&item.size), sizeof(item.size));
        crc = crc32(crc, reinterpret_cast<const Bytef*>(&item.size), sizeof(item.size));
    }

    pak->crc_value = crc;
    stream.write(reinterpret_cast<const char*>(&pak->crc_value), sizeof(pak->crc_value));
    stream.write(reinterpret_cast<const char*>(&index_begin), sizeof(index_begin));
}

//-----------------------------------------------------------------------------
paker::paker()
: version(plpaker_version) {
}

//-----------------------------------------------------------------------------
bool paker::unpack(pak::ptr pak,
                   fs::path const& pak_file,
                   fs::path const& output_path) const {
    std::ifstream file(pak_file, std::ios::binary);
    if (!file) {
        on_log_error(std::format("cannot read file: {}", pak_file.string()));
        return false;
    }

    int64_t const max_data_size = std::pow(2, std::ceil(std::log2(pak->max_size)));

    scope_data data_compressed(max_data_size);
    scope_data data_decompressed(max_data_size);

    for (auto& item : pak->items) {
        if (!valid_parameter(item))
            continue;

        auto data_size = item.end - item.begin;

        auto data_file = output_path;
        data_file += fs::path::preferred_separator;
        data_file += item.filename;

        on_log_info(std::format("{} - {}", item.index, item.filename));

        auto const parent_path = data_file.parent_path();
        if (!fs::exists(parent_path)) {
            if (!fs::create_directories(parent_path)) {
                on_log_error(std::format("cannot create folder: {}", parent_path.string()));
                return false;
            }
        }

        file.seekg(item.begin);
        file.read(data_compressed.ptr, data_size);

        auto data = data_compressed.ptr;

        auto data_target_file = data_file;
        if (item.compressed)
            data_target_file += compressed_extension;

        std::ofstream target_file(data_target_file, std::ios::binary);
        if (!target_file) {
            on_log_error(std::format("cannot write file: {}", data_target_file.string()));
            return false;
        }

        target_file.write(data_compressed.ptr, data_size);
        target_file.close();

        if (item.compressed && options.decompress) {
            size_t compressed_data_size = data_size;
            size_t decompressed_data_size = max_data_size;

            if (!decompress_data(data_compressed.ptr,
                                 compressed_data_size,
                                 data_decompressed.ptr,
                                 decompressed_data_size)) {
                on_log_error(std::format("decompress file: {}", data_file.string()));
                return false;
            }

            item.size_compressed = compressed_data_size;

            std::ofstream decompressed_file(data_file, std::ios::binary);
            if (!decompressed_file) {
                on_log_error(std::format("cannot write file: {}", data_file.string()));
                return false;
            }

            decompressed_file.write(data_decompressed.ptr, decompressed_data_size);
            decompressed_file.close();

            data = data_decompressed.ptr;
        }
    }

    file.close();
    return true;
}

//-----------------------------------------------------------------------------
bool paker::pack(pak::ptr pak,
                 fs::path const& input_path,
                 fs::path const& output_file) const {
    std::ofstream pak_file(output_file, std::ios::binary);
    if (!pak_file) {
        on_log_error(std::format("cannot write file: {}", output_file.string()));
        return false;
    }

    int64_t const max_data_size = std::pow(2, std::ceil(std::log2(pak->max_size)));
    scope_data data(max_data_size);

    uLong crc = crc32(0L, Z_NULL, 0);

    for (auto& item : pak->items) {
        if (!valid_parameter(item))
            continue;

        on_log_info(std::format("{} - {}", item.index, item.filename));

        auto data_file = input_path;
        data_file += fs::path::preferred_separator;
        data_file += item.filename;

        auto data_target_file = data_file;
        if (item.compressed)
            data_target_file += compressed_extension;

        if (options.decompress && item.compressed) {
            std::ifstream decompressed_file(data_file, std::ios::binary);
            if (!decompressed_file) {
                on_log_error(std::format("cannot read file: {}", data_file.string()));
                return false;
            }

            scope_data compressed_data(max_data_size);
            size_t compressed_data_size = max_data_size;

            decompressed_file.seekg(0, decompressed_file.end);
            size_t decompressed_size = decompressed_file.tellg();
            decompressed_file.seekg(0, decompressed_file.beg);

            decompressed_file.read(data.ptr, decompressed_size);
            decompressed_file.close();

            item.size = decompressed_size;

            if (!compress_data(data.ptr,
                               decompressed_size,
                               compressed_data.ptr,
                               compressed_data_size)) {
                on_log_error(std::format("compress file: {}", data_file.string()));
                return false;
            }

            item.size_compressed = compressed_data_size;

            if (fs::exists(data_target_file))
                fs::remove(data_target_file);

            std::ofstream compressed_file(data_target_file, std::ios::binary);
            if (!compressed_file) {
                on_log_error(std::format("cannot write file: {}", data_target_file.string()));
                return false;
            }

            compressed_file.write(compressed_data.ptr, compressed_data_size);
            compressed_file.close();
        }

        std::ifstream target_file(data_target_file, std::ios::binary);
        if (!target_file) {
            on_log_error(std::format("cannot read file: {}", data_target_file.string()));
            return false;
        }

        target_file.seekg(0, target_file.end);
        size_t const target_size = target_file.tellg();
        target_file.seekg(0, target_file.beg);

        target_file.read(data.ptr, target_size);

        item.begin = pak_file.tellp();

        pak_file.write(data.ptr, target_size);
        crc = crc32(crc, reinterpret_cast<const Bytef*>(data.ptr), target_size);

        item.end = pak_file.tellp();
    };

    write_index(pak, pak_file, crc);

    pak_file.close();
    return true;
}

//-----------------------------------------------------------------------------
bool paker::patch_files(fs::path const& pak_file,
                        fs::path const& output_file,
                        string_list const& files) const {
    auto pak = pak::create();
    if (!pak->parse(pak_file)) {
        on_log_error(std::format("cannot parse file: {}", pak_file.string()));
        return false;
    }

    std::ifstream input(pak_file, std::ios::binary);
    if (!input) {
        on_log_error(std::format("cannot read file: {}", pak_file.string()));
        return false;
    }

    std::ofstream output(output_file, std::ios::binary);
    if (!output) {
        on_log_error(std::format("cannot write file: {}", output_file.string()));
        return false;
    }

    int64_t const max_data_size = std::pow(2, std::ceil(std::log2(pak->max_size)));
    scope_data data(max_data_size);

    uLong crc = crc32(0L, Z_NULL, 0);

    for (auto& item : pak->items) {
        bool patch = false;

        for (auto const& file : files) {
            if (item.filename.contains(fs::path(file).filename().string())) {
                on_log_info(std::format("{} - {}", item.index, item.filename));

                if (item.compressed) {
                    std::ifstream decompressed_file(file, std::ios::binary);
                    if (!decompressed_file) {
                        on_log_error(std::format("cannot read file: {}", file));
                        return false;
                    }

                    scope_data decompressed_data(max_data_size);
                    size_t compressed_data_size = max_data_size;

                    decompressed_file.seekg(0, decompressed_file.end);
                    size_t decompressed_size = decompressed_file.tellg();
                    decompressed_file.seekg(0, decompressed_file.beg);

                    decompressed_file.read(decompressed_data.ptr, decompressed_size);
                    decompressed_file.close();

                    if (!compress_data(decompressed_data.ptr,
                                       decompressed_size,
                                       data.ptr,
                                       compressed_data_size)) {
                        on_log_error(std::format("compress file: {}", file));
                        return false;
                    }

                    item.size_compressed = compressed_data_size;
                    item.size = decompressed_size;
                } else {
                    std::ifstream uncompressed_file(file, std::ios::binary);
                    if (!uncompressed_file) {
                        on_log_error(std::format("cannot read file: {}", file));
                        return false;
                    }

                    uncompressed_file.seekg(0, uncompressed_file.end);
                    size_t uncompressed_size = uncompressed_file.tellg();
                    uncompressed_file.seekg(0, uncompressed_file.beg);

                    uncompressed_file.read(data.ptr, uncompressed_size);
                    uncompressed_file.close();

                    item.size_compressed = uncompressed_size;
                    item.size = uncompressed_size;
                }

                patch = true;
                break;
            }
        }

        int64_t data_size = 0;
        if (patch) {
            data_size = item.size_compressed;
        } else {
            data_size = item.end - item.begin;

            input.seekg(item.begin);
            input.read(data.ptr, data_size);
        }

        item.begin = output.tellp();

        output.write(data.ptr, data_size);
        crc = crc32(crc, reinterpret_cast<const Bytef*>(data.ptr), data_size);

        item.end = output.tellp();
    };

    write_index(pak, output, crc);

    output.close();
    input.close();
    return true;
}

//-----------------------------------------------------------------------------
bool paker::valid_parameter(pak::item const& item) const {
    if ((parameters.start != 0) && (item.index < parameters.start))
        return false;
    if ((parameters.end != 0) && (item.index > parameters.end))
        return false;
    if (!parameters.filter.empty() && !item.filename.contains(parameters.filter))
        return false;
    return true;
}

} // namespace pl
