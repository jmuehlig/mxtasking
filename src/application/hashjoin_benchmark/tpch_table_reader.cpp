#include "tpch_table_reader.h"
#include <fstream>
#include <sstream>

using namespace application::hash_join;
void TPCHTableReader::read(const std::string &file_name,
                           std::function<void(const std::uint16_t, const std::string &)> &&callback)
{
    std::ifstream tpc_file(file_name);
    if (tpc_file.good())
    {
        std::string line;
        while (std::getline(tpc_file, line))
        {
            auto line_stream = std::stringstream{line};
            std::string column;
            auto index = 0U;
            while (std::getline(line_stream, column, '|'))
            {
                callback(index++, column);
            }
        }
    }
}