/**
 * This main module consists of a simple CLI for testing the ffsys::FFSys class.
 */
#include "ffsys.hh"
#include "utilities.hh"

#include <iostream>

using namespace std;

void print_error(ffsys::ErrorNumber errnum);

int main() {
    try {
        ffsys::FFSys* fs;

        string input = "";
        cout << "Create or open existing? (C/O): ";
        getline(cin, input);

        cout << "Give FFSys file name (empty for default \"test.ffsys\"): ";
        string name = "";
        getline(cin, name);

        if (name.empty()) {
            name = "test.ffsys";
        }

        input = Utilities::string_to_upper(input);
        if (input.starts_with('C')) {

            // Ask for block size.
            int block_size = 1024;
            cout << "Block size in bytes (1024): ";
            getline(cin, input);
            if (!input.empty()) {
                if (!Utilities::is_int(input)) {
                    cout << "Block size is not an integer, using default size 1024." << endl;
                } else {
                    block_size = stoi(input);
                }
            }

            fs = new ffsys::FFSys(name, block_size);
        } else if (input.starts_with("O")) {
            fs = new ffsys::FFSys(name);
        } else {
            cout << "Incorrect file open option, defaulting to opening existing." << endl;
            fs = new ffsys::FFSys(name);
        }

        cout << endl << "Input \"help\" for list of commands.";

        input = "";
        while (true) {
            // Get user input
            cout << endl << "cmd> ";
            getline(cin, input);
            if (input == "quit") {
                break;
            }

            // Split input
            vector<string> split_input = Utilities::split(input, ' ');
            if (split_input.empty()) {
                cout << endl;
                continue;
            }

            string cmd = Utilities::string_to_lower(split_input.at(0));
            vector<string> params(split_input.begin()+1, split_input.end());

            // HELP command
            if (cmd == "help") {
                cout
                    << "Available commands: " << endl
                    << " - help" << endl << endl

                    << " - open <filename> <flag(trunc|end|create)?>" << endl
                    << " - write <fd> <file_name> <count?>" << endl
                    << " - read <fd> <dest_file> <count>" << endl
                    << " - close <fd>" << endl
                    << " - seek <fd> <pos>" << endl << endl

                    << " - stats" << endl
                    << " - files" << endl
                    << " - open_files" << endl;
            }

            // OPEN command
            else if (cmd == "open") {
                if (params.empty() or params.size() > 2) {
                    cout << "Error: wrong N params!" << endl;
                    continue;
                }

                int openflag = 0;
                if (params.size() == 2) {
                    string flag = Utilities::string_to_lower(params.at(1));
                    if (flag == "trunc") {
                        openflag = ffsys::OpenFlags::TRUNCATE;
                    } else if (flag == "end") {
                        openflag = ffsys::OpenFlags::END;
                    } else if (flag == "create") {
                        openflag = ffsys::OpenFlags::CREATE;
                    } else {
                        cout << "Error: unknown flag param!" << endl;
                        continue;
                    }
                }

                ffsys::file_descriptor fd = fs->open(params.at(0), openflag);
                if (fd == -1) {
                    print_error(fs->errnum());
                } else {
                    cout << "Opened file " << params.at(0) << ". FD: " << fd << endl;
                }
            }

            // WRITE command
            else if (cmd == "write") {
                if (params.size() < 2) {
                    cout << "Error: wrong N params!" << endl;
                    continue;
                }

                if (!Utilities::is_int(params.at(0))) {
                    cout << "Error: file descriptor is not integer!" << endl;
                    continue;
                }
                ffsys::file_descriptor fd = stoi(params.at(0));

                ifstream file(params.at(1), ios_base::ate);
                if (!file) {
                    cout << "Error: could not open file!" << endl;
                    continue;
                }

                unsigned int to_read = file.tellg();
                if (params.size() == 3 and Utilities::is_int(params.at(2))) {
                    to_read = stoi(params.at(2));
                }

                // Read file
                char* buffer = new char[to_read];
                file.seekg(0);
                file.read(buffer, to_read);

                size_t count = fs->write(fd, buffer, to_read);
                if (count == -1) {
                    print_error(fs->errnum());
                } else {
                    cout << "Wrote " << count << " bytes into file." << endl;
                }

                // Clean up
                delete[] buffer;
                file.close();
            }

            // READ command
            else if (cmd == "read") {
                if (params.size() < 3) {
                    cout << "Error: wrong N params!" << endl;
                    continue;
                }

                if (!Utilities::is_int(params.at(0))) {
                    cout << "Error: file descriptor is not integer!" << endl;
                    continue;
                }
                ffsys::file_descriptor fd = stoi(params.at(0));

                ofstream file(params.at(1));
                if (!file) {
                    cout << "Error: could not open file!" << endl;
                    continue;
                }

                if (!Utilities::is_int(params.at(2))) {
                    cout << "Error: count is not integer!" << endl;
                    continue;
                }
                unsigned int to_read = stoi(params.at(2));

                // Read ffile
                char* buffer = new char[to_read];
                size_t count = fs->read(fd, buffer, to_read);
                if (count == -1) {
                    print_error(fs->errnum());
                } else {
                    cout << "Read " << count << " bytes from file." << endl;
                }

                // Write to file
                file.write(buffer, count);

                // Clean up
                delete[] buffer;
                file.close();
            }

            // CLOSE command
            else if (cmd == "close") {
                if (params.size() != 1) {
                    cout << "Error: wrong N params!" << endl;
                    continue;
                }

                if (!Utilities::is_int(params.at(0))) {
                    cout << "Error: file descriptor is not integer!" << endl;
                    continue;
                }

                if (!fs->close(stoi(params.at(0)))) {
                    print_error(fs->errnum());
                }
            }

            // SEEK command
            else if (cmd == "seek") {
                if (params.size() != 2) {
                    cout << "Error: wrong N params!" << endl;
                    continue;
                }

                if (!Utilities::is_int(params.at(0))) {
                    cout << "Error: file descriptor is not integer!" << endl;
                    continue;
                }
                if (!Utilities::is_int(params.at(1))) {
                    cout << "Error: file position is not integer!" << endl;
                    continue;
                }

                if (!fs->seek(stoi(params.at(0)), stoi(params.at(1)))) {
                    print_error(fs->errnum());
                    continue;
                }
            }

            // Stat commands
            else if (cmd == "stats") {
                fs->print_superblock();
            }
            else if (cmd == "files") {
                fs->print_all_files();
            }
            else if (cmd == "open_files") {
                fs->print_open_files();
            }

            else {
                cout << "Error: Unknown command!" << endl;
            }
        }

        delete fs;

    } catch (string error) {
        cout << error << endl;
    }

    return EXIT_SUCCESS;
}


void print_error(ffsys::ErrorNumber errnum) {
    switch (errnum) {
    case ffsys::ErrorNumber::CANT_READ_INODE:
        cout << "CANT_READ_INODE" << endl;
        break;
    case ffsys::ErrorNumber::FILE_ALREADY_EXISTS:
        cout << "FILE_ALREADY_EXISTS" << endl;
        break;
    case ffsys::ErrorNumber::NO_ERROR:
        cout << "NO_ERROR" << endl;
        break;
    case ffsys::ErrorNumber::NO_FREE_INODES:
        cout << "NO_FREE_INODES" << endl;
        break;
    case ffsys::ErrorNumber::NO_FREE_DATA_BLOCKS:
        cout << "NO_FREE_DATA_BLOCKS" << endl;
        break;
    case ffsys::ErrorNumber::NO_SUCH_FILE_DESCRIPTOR:
        cout << "NO_SUCH_FILE_DESCRIPTOR" << endl;
        break;
    case ffsys::ErrorNumber::PATH_NOT_FOUND:
        cout << "PATH_NOT_FOUND" << endl;
        break;
    case ffsys::ErrorNumber::NO_SUCH_FILE:
        cout << "NO SUCH FILE" << endl;
        break;
    case ffsys::ErrorNumber::FILE_ALREADY_OPEN:
        cout << "FILE_ALREADY_OPEN" << endl;
    }
}
