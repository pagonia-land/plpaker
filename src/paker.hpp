#pragma once

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace pl {

namespace fs = std::filesystem;
using string = std::string;
using string_list = std::vector<string>;

struct pak {
    using ptr = std::shared_ptr<pak>;
    using list = std::vector<ptr>;

    static ptr create() {
        return std::make_shared<pak>();
    }

    struct item {
        using list = std::vector<item>;

        uint32_t index = 0; // list position
        uint64_t pos = 0;   // index address

        bool compressed = false; // data mode
        string filename;         // 128+ offset

        int64_t begin = 0; // start address
        int64_t end = 0;   // last address

        int64_t size = 0;            // target data size (decompressed)
        int64_t size_compressed = 0; // without header/padding
    };

    item::list items;     // pak info
    int64_t max_size = 0; // item data size

    int64_t length = 0; // pak data size

    uint64_t crc_pos = 0;   // crc32 address
    uint32_t crc_value = 0; // crc32 number

    uint64_t index_pos = 0;  // info address
    int64_t index_begin = 0; // index address

    int32_t version = 0; // increment
    int32_t count = 0;   // items size

    bool parse(fs::path const& pak_file);
    bool load(fs::path const& pakinfo_file);

    bool write_info(fs::path const& output_path) const;
};

bool compress_data(char* const decompressed_data,
                   size_t& decompressed_data_size,
                   char* compressed_data,
                   size_t& compressed_data_size);

bool compress_file(fs::path const& input_file,
                   fs::path const& output_file);

bool decompress_data(char* const compressed_data,
                     size_t& compressed_data_size,
                     char* decompressed_data,
                     size_t& decompressed_data_size);

bool decompress_file(fs::path const& input_file,
                     fs::path const& output_file);

struct paker {
    string version;

    using log_func = std::function<void(string const&)>;
    log_func on_log_info;
    log_func on_log_error;

    pak::list paks;

    struct options {
        bool compress = true;
        bool decompress = true;
        bool png = true;
    };
    options options;

    struct parameters {
        uint32_t start = 0;
        uint32_t end = 0;
        string filter;
    };
    parameters parameters;

    explicit paker();

    bool unpack(pak::ptr pak,
                fs::path const& pak_file,
                fs::path const& output_path) const;

    bool pack(pak::ptr pak,
              fs::path const& input_path,
              fs::path const& output_file) const;

    bool patch_files(fs::path const& pak_file,
                     fs::path const& output_file,
                     string_list const& files) const;

    bool valid_parameter(pak::item const& item) const;
};

} // namespace pl
