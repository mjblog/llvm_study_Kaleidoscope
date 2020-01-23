#include <iostream>
#include <fstream>
#include <string>
namespace toy_compiler{

extern void err_print(bool is_fatal, const std::string & msg);

enum token_type
{
    token_def,
    token_extern,
    token_identifier,
    token_number,
    token_eof,
    token_wrong
};
typedef enum token_type token_type_t;
class token
{
    std::string raw_str;
/*
    丢弃了原示例中的NumVal，token应该只管str级别的数据
    另外由于单个token只用一次，用的时候再转也并不会影响效率
*/
public:
    token_type_t type;
    inline token_type_t get_type() {return type;}
    static inline token_type_t identifier_string_to_type (const std::string &input)
    {
        if (input == "def")
            return token_def;

        if (input == "extern")
            return token_extern;
        
        return token_identifier;
    }
    inline std::string & get_str() {return raw_str;};
};

class lexer
{
    std::istream *input_stream = nullptr;
    std::string error_msg;
    token  cur_token;
    static inline int get_next_char(std::istream *in)
    {
        return in->get(); 
    }
    static inline void skip_spaces(std::istream *in, int & cur_char)
    {
        if (isspace(cur_char))
        {
            do
            {
                cur_char = get_next_char(in);
            }while (isspace(cur_char));
        }
    }
    static inline void process_comments(std::istream *in, int & cur_char)
    {
        if (cur_char == '#')
        {
            do
            {
               cur_char = get_next_char(in);
            }while (cur_char != EOF && cur_char != '\r' && cur_char != '\n' );
        }
    }
    inline bool get_identifier(std::istream *in, int & cur_char) 
    {
        if (isalpha(cur_char))
        {
            std::string cur_str = cur_token.get_str();
            cur_str.clear();
            cur_str += cur_char;
            cur_char = get_next_char(in);
            while (isalnum(cur_char))
            {
                cur_str += cur_char;
                cur_char = get_next_char(in);
            }
            cur_token.type = cur_token.identifier_string_to_type(cur_str);
            return true;
        }
        else
            return false;
    }
    inline bool get_number(std::istream *in, int & cur_char) 
    {
        //改进原示例，数字不能以 ' . ' 开头，只能出现一次' . ' 
        if (isdigit(cur_char))
        {
            std::string cur_str = cur_token.get_str();
            cur_str.clear();
            cur_str += cur_char;
            cur_char = get_next_char(in);
            bool seen_dot = false;
            while (isdigit(cur_char) || (cur_char == '.'))
            {
                if (cur_char == '.') 
                {
                    if (seen_dot)
                        err_print(/*isfatal*/true, 
                            "a number shoud not contain multiple dots : " + cur_str);
                    else
                        seen_dot = true;
                }

                cur_str += cur_char;
                cur_char = get_next_char(in);
            }
        }
        else
            return false;
    }
public:
        bool is_ok = true;
        inline const token & get_cur_token() {return cur_token;}
        void get_next_token()
        {
            int cur_char = get_next_char(input_stream);
            
            while (cur_char != EOF)
            {
                process_comments(input_stream, cur_char);
                skip_spaces(input_stream, cur_char);
                if (get_identifier(input_stream, cur_char))
                    return;

                if (get_number(input_stream, cur_char))
                    return;
            }
            
        }
        lexer(const std::string & filename)
        {
            auto *fstream  = new std::ifstream;
            fstream->open(filename, std::ios_base::in);
            if (fstream->is_open())
            {
                input_stream = fstream;
            }
            else
            {
                error_msg = "can not open input file" + filename;
                is_ok = false;
            }
        }
        lexer()
        {
            input_stream = &std::cin;
        }
        ~lexer()
        {
            //依赖stream的析构close
            if (input_stream != &std::cin && input_stream)
                delete input_stream;
        }
};


}   // end of namespace toy_compiler