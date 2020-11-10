#include "profile.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <functional>
#include <iostream>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

using namespace std;

uint64_t memory_size = 500ULL * 1024 * 1024;
uint64_t max_block_size = memory_size / 30;
string sorting_type;
size_t sorting_column;
int K = 4;

string FileName(int merge_level, int block_number) {
    return "__level-" + to_string(merge_level) + "_block-" + to_string(block_number);
}

struct Field {
    string value;
    bool have_quotes = true;
};

struct Line {
    uint64_t size = 0;
    vector<Field> data;
};

using Block = vector<Line>;

bool CompareLines(const Line *lhs, const Line *rhs) {
    if (sorting_type == "int") {
        return stoi(lhs->data[sorting_column].value) <
               stoi(rhs->data[sorting_column].value);
    } else if (sorting_type == "float") {
        return stof(lhs->data[sorting_column].value) <
               stof(rhs->data[sorting_column].value);
    } else if (sorting_type == "string") {
        return lhs->data[sorting_column].value <
               rhs->data[sorting_column].value;
    } else {
        throw invalid_argument("Wrong sorting type: " + sorting_type);
    }
}

vector<const Line *> BlockSortedPointers(const Block &block) {
    vector<const Line *> lines_p(block.size());
    for (int i = 0; i < block.size(); ++i) {
        lines_p[i] = &block[i];
    }
    sort(lines_p.begin(), lines_p.end(), CompareLines);
    return lines_p;
}

void WriteLine(ofstream &out, const Line &line) {
    bool first_field = true;
    for (const Field &field : line.data) {
        if (!first_field)
            out << ',';
        first_field = false;
        if (field.have_quotes)
            out << '\"';
        out << field.value;
        if (field.have_quotes)
            out << '\"';
    }
    out << '\n';
}

void WriteBlock(ofstream &out, const vector<const Line *> &lines_p) {
    for (const Line *line_p : lines_p) {
        WriteLine(out, *line_p);
    }
}

void WriteBlock(ofstream &out, const Block &block) {
    for (const Line &line : block) {
        WriteLine(out, line);
    }
}

bool ReadBlock(ifstream &in, Block &block) {
    block.clear();
    istringstream parse_stream;
    string input_line;
    uint64_t cur_block_size = 0;
    bool first_line_read = false;
    while (getline(in, input_line)) {
        if (cur_block_size + input_line.size() > max_block_size) {
            in.seekg(-static_cast<int>(input_line.size()) - 1, ios_base::cur);
            if (!first_line_read)
                throw logic_error("The block size is not enough for 1 line");
            return true;
        }
        first_line_read = true;
        cur_block_size += input_line.size();
        parse_stream.clear();
        parse_stream.str(input_line);
        block.emplace_back();
        Field field;
        while (true) {
            char delimiter;
            if (parse_stream.peek() == '\"') {
                parse_stream.ignore(1);
                field.have_quotes = true;
                delimiter = '\"';
            } else {
                field.have_quotes = false;
                delimiter = ',';
            }
            if (!getline(parse_stream, field.value, delimiter)) {
                break;
            }
            if (field.have_quotes) {
                parse_stream.ignore(1);
            }
            block.back().data.push_back(field);
        }
        block.back().size = input_line.size();
    }
    return cur_block_size > 0;
}

vector<uint32_t> files_per_level;

void SortBlocks(const string &table_name) {
    ifstream table_in(table_name, ios::in);
    ofstream sorted_blocks_out;
    Block cur_block;
    files_per_level.push_back(0);
    while (ReadBlock(table_in, cur_block)) {
        sorted_blocks_out.open(FileName(0, files_per_level[0]), ios::out);
        WriteBlock(sorted_blocks_out, BlockSortedPointers(cur_block));
        ++files_per_level[0];
        sorted_blocks_out.close();
    }
}

class LineInputStream {
public:
    explicit LineInputStream(const string &filename) {
        in.open(filename);
    }

    bool eof() const {
        return block_index == block.size() && in.eof();
    }

    Line &NextLine() {
        if (block_index == block.size()) {
            ReadBlock(in, block);
            block_index = 0;
        }
        return block[block_index++];
    }

private:
    ifstream in;
    Block block;
    size_t block_index = 0;
};

void MergeFiles(vector<LineInputStream> &streams, int merge_level) {
    priority_queue<pair<Line *, int>,
            vector<pair<Line *, int>>,
            function<bool(pair<Line *, int>, pair<Line *, int>)>> heap(
            [](pair<Line *, int> lhs, pair<Line *, int> rhs) {
                return CompareLines(rhs.first, lhs.first);
            });
    ofstream merged_out;
    merged_out.open(FileName(merge_level + 1, files_per_level[merge_level + 1]));
    Block merged_block;
    uint64_t merged_block_size = 0;
    for (int i = 0; i < streams.size(); ++i) {
        heap.push({&streams[i].NextLine(), i});
    }
    while (!heap.empty()) {
        auto[max_line_p, index] = heap.top();
        heap.pop();
        if (merged_block_size + max_line_p->size > max_block_size) {
            WriteBlock(merged_out, merged_block);
            merged_block.clear();
            merged_block_size = 0;
        }
        merged_block_size += max_line_p->size;
        merged_block.push_back(*max_line_p);
        if (!streams[index].eof()) {
            heap.push({&streams[index].NextLine(), index});
        }
    }
    if (merged_block_size > 0) {
        WriteBlock(merged_out, merged_block);
    }
}

int Merge(int merge_level) {
    uint32_t blocks_to_merge = (K * memory_size / 10) / max_block_size;
    if (blocks_to_merge < 3) {
        throw logic_error("Not enough memory for 3 blocks");
    }
    --blocks_to_merge; // 1 block is result;
    int merged_files = 0;
    files_per_level.push_back(0);
    while (merged_files < files_per_level[merge_level]) {
        vector<LineInputStream> streams;
        for (int i = merged_files; i < min(files_per_level[merge_level],
                                           merged_files + blocks_to_merge); ++i) {
            streams.emplace_back(FileName(merge_level, i));
        }
        MergeFiles(streams, merge_level);
        merged_files += blocks_to_merge;
        ++files_per_level[merge_level + 1];
    }
    ++merge_level;
    if (files_per_level[merge_level] == 1) {
        return merge_level;
    } else if (files_per_level[merge_level] == 0) {
        throw logic_error("No files to merge");
    } else {
        return Merge(merge_level);
    }
}

int main(int argc, char *argv[]) {
    if (argc < 4) {
        throw invalid_argument("Not enough arguments");
    }
    string table_name = argv[1];
    sorting_column = stoi(argv[2]);
    sorting_type = argv[3];
    if (argc > 4) {
        memory_size = 1024ULL * 1024 * stoi(argv[4]);
    }
    if (argc > 5) {
        K = stoi(argv[5]);
    }
    {
        LOG_DURATION("Parameters: memory_size = " + to_string(memory_size / (1024 * 1024)) +
                     "Mb, max_block_size = " + to_string(max_block_size / (1024 * 1024)) +
                     "Mb\nTime");
        SortBlocks(table_name);
        Merge(0);
    }
    rename(FileName(static_cast<int>(files_per_level.size()) - 1, 0).c_str(),
           (table_name.substr(0, table_name.rfind('.')) + "_sorted.csv").c_str());
    for (int i = 0; i + 1 < files_per_level.size(); ++i) {
        for (int j = 0; j < files_per_level[i]; ++j) {
            remove(FileName(i, j).c_str());
        }
    }
}
