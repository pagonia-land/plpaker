#include "argh.h"
#include "paker.hpp"
#include <format>
#include <iostream>

using namespace std;
using namespace pl;

//-----------------------------------------------------------------------------
int main(int, char* argv[]) {
    paker paker;
    cout << format("Pagonia Land - Packing Tool - PLPaker v{}", paker.version) << endl;

    auto show_help = []() {
        cout << endl;
        cout << "usage:  plpaker <command> <args> [<options>] [<parameters>] " << endl;
        cout << endl;
        cout << "commands:" << endl;
        cout << "  list <pak> [<dir>]           # Parse pak file and write pakinfo.json" << endl;
        cout << endl;
        cout << "  unpack <pak> [<dir>]         # Unpack pak file into the folder" << endl;
        cout << "  pack <pakinfo> [<pak>]       # Pack new pak file based on pakinfo.json" << endl;
        cout << endl;
        cout << "  compress <file> <out>        # Compress a file" << endl;
        cout << "  decompress <file> <out>      # Decompress a file" << endl;
        cout << endl;
        cout << "  patch <pak> <out> <files>    # Repack pak file with files to be replaced" << endl;
        cout << endl;
        cout << "options:" << endl;
        cout << "  -c | --compress       # Unpack/Pack compressed files" << endl;
        cout << "  -d | --decompress     # Unpack/Pack decompressed files" << endl;
        cout << endl;
        cout << "If no options are specified, all options are active, otherwise only the set ones." << endl;
        cout << endl;
        cout << "parameters:" << endl;
        cout << "  -s | --start      # Start index:    -s=38" << endl;
        cout << "  -e | --end        # End index:      -e=1602" << endl;
        cout << "  -f | --filter     # Name filter:    -f=gold" << endl;
        cout << endl;
        cout << "All parameters work on unpack and pack commands." << endl;
        cout << endl;
        cout << "Need help? Please feel free to ask us on Discord: https://Pagonia.Land" << endl;
    };

    paker.on_log_info = [](string const& msg) {
        cout << msg << endl;
    };
    paker.on_log_error = [](string const& msg) {
        cerr << msg << endl;
    };

    argh::parser cmd_line(argv);
    if ((cmd_line.pos_args().size() < 2) || cmd_line[{"-h", "--help"}]) {
        show_help();
        return 0;
    }

    if (!cmd_line.flags().empty()) {
        paker.options.compress = cmd_line[{"-d", "--compress"}];
        paker.options.decompress = cmd_line[{"-d", "--decompress"}];
    }

    cmd_line({"-s", "--start"}) >> paker.parameters.start;
    cmd_line({"-e", "--end"}) >> paker.parameters.end;
    cmd_line({"-f", "--filter"}) >> paker.parameters.filter;

    auto const command = cmd_line[1];
    auto const input = cmd_line[2];
    auto const output = cmd_line[3];

    auto parse_pak = [&]() -> pak::ptr {
        if (input.empty()) {
            cerr << "no pak file set" << endl;
            show_help();
            return nullptr;
        }

        auto pak = pak::create();
        if (!pak->parse(input)) {
            cerr << "cannot parse file" << endl;
            return nullptr;
        }

        return pak;
    };

    auto prepare_output_path = [&](bool clean) -> fs::path {
        fs::path output_path = output;
        if (output_path.empty()) {
            output_path = fs::current_path();
            output_path += fs::path::preferred_separator;

            fs::path const filename = input;
            output_path += filename.stem().string() + "_pak";
        }

        if (clean && fs::exists(output_path))
            fs::remove_all(output_path);

        if (!fs::exists(output_path) && !fs::create_directories(output_path)) {
            cerr << format("cannot create folder: {}", output_path.string()) << endl;
            return {};
        }

        return output_path;
    };

    if ((command == "list") || (command == "ls")) {
        auto pak = parse_pak();
        if (!pak)
            return -1;

        auto const output_path = prepare_output_path(false);
        if (output_path.empty())
            return -1;

        if (!pak->write_info(output_path)) {
            cerr << "cannot write info" << endl;
            return -1;
        }

        cout << "ready: pakinfo.json" << endl;
        return 0;
    }

    if ((command == "unpack") || (command == "u")) {
        auto pak = parse_pak();
        if (!pak)
            return -1;

        auto const output_path = prepare_output_path(true);
        if (output_path.empty())
            return -1;

        if (!paker.unpack(pak, input, output_path)) {
            cerr << "cannot unpack" << endl;
            return -1;
        }

        if (!pak->write_info(output_path)) {
            cerr << "cannot write info" << endl;
            return -1;
        }

        cout << "unpacked." << endl;
        return 0;
    }

    if ((command == "pack") || (command == "p")) {
        fs::path const pakinfo_file = input;
        if (pakinfo_file.empty()) {
            cerr << "no pak info file set" << endl;
            show_help();
            return -1;
        }

        auto pak = pak::create();
        if (!pak->load(pakinfo_file)) {
            cerr << "cannot load file" << endl;
            return -1;
        }

        auto const input_path = pakinfo_file.parent_path();

        fs::path output_file = output;
        if (output_file.empty()) {
            output_file = fs::current_path();
            output_file += fs::path::preferred_separator;
            output_file += "core.pak";
        }

        auto const parent_path = output_file.parent_path();
        if (!fs::exists(parent_path)) {
            if (!fs::create_directories(parent_path)) {
                cerr << format("cannot create folder: {}", parent_path.string()) << endl;
                return -1;
            }
        }

        if (fs::exists(output_file))
            fs::remove(output_file);

        if (!paker.pack(pak, input_path, output_file)) {
            cerr << "cannot pack" << endl;
            return -1;
        }

        cout << "packed." << endl;
        return 0;
    }

    auto prepare_output_file = [&]() -> fs::path {
        if (output.empty()) {
            cerr << "no output file set" << endl;
            show_help();
            return {};
        }

        fs::path const output_file = output;
        auto const parent_path = output_file.parent_path();
        if (!fs::exists(parent_path)) {
            if (!fs::create_directories(parent_path)) {
                cerr << format("cannot create folder: {}", parent_path.string()) << endl;
                return {};
            }
        }

        return output_file;
    };

    if ((command == "compress") || (command == "c")) {
        auto const output_file = prepare_output_file();
        if (output_file.empty())
            return -1;

        if (!compress_file(input, output_file)) {
            cerr << "cannot compress" << endl;
            return -1;
        }

        cout << "compressed." << endl;
        return 0;
    }

    if ((command == "decompress") || (command == "d")) {
        auto const output_file = prepare_output_file();
        if (output_file.empty())
            return -1;

        if (!decompress_file(input, output_file)) {
            cerr << "cannot decompress" << endl;
            return -1;
        }

        cout << "decompressed." << endl;
        return 0;
    }

    if ((command == "patch") || (command == "x")) {
        auto const output_file = prepare_output_file();
        if (output_file.empty())
            return -1;

        if (cmd_line.pos_args().size() < 5) {
            cerr << "no files set" << endl;
            show_help();
            return -1;
        }

        string_list files;
        for (auto i = 4u; i < cmd_line.pos_args().size(); ++i)
            files.push_back(cmd_line[i]);

        if (!paker.patch_files(input, output_file, files)) {
            cerr << "cannot patch" << endl;
            return -1;
        }

        cout << "patched." << endl;
        return 0;
    }

    show_help();
    return -1;
}
