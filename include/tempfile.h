#ifndef _TEMPFILE_H_
#define _TEMPFILE_H_
#include <iostream>
#include <string>
#include <cstring>
#include <cstdio>
#include <cerrno>
namespace toy_compiler{
using namespace std;
class tempfile_with_content final 
{
    bool is_opened = false;
    string err_msg;
    FILE * file_struct = nullptr;
public:
    tempfile_with_content(const string & file_content)
    {
        char tmpbuf[1024];
        file_struct = tmpfile();
        if (file_struct != nullptr)
        {
            is_opened = true;
            fwrite(file_content.c_str(), file_content.length(),
                            1, file_struct);
            int write_err = ferror(file_struct);
            if (write_err)
                err_msg = strerror_r(write_err, tmpbuf, sizeof(tmpbuf));
        }
        else
        {
            is_opened = false;
            err_msg = strerror_r(errno, tmpbuf, sizeof(tmpbuf));
        }
        
    }

    ~tempfile_with_content()
    {
        if (file_struct != nullptr)
            fclose(file_struct);
    }

};

}
#endif