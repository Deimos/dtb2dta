#define BUFFER_SIZE 4
#define HEADER_SIZE 7

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void parse_tree(FILE*, FILE*, int, int, int);
void print_tabs(FILE*, int);
char get_open_bracket(int);
char get_close_bracket(int);

void parse_string_val(FILE*, FILE*);
void parse_int_val(FILE*, FILE*);
void parse_float_val(FILE*, FILE*);
void parse_directive(FILE*, FILE*, char*, int);

int main(int argc, char **argv)
{
    char* buffer = malloc(sizeof(char) * HEADER_SIZE);
    FILE* dtb_file;
    FILE* out_file;
    
    dtb_file = fopen(argv[1], "r");
    argv[1][strlen(argv[1])-1] = 'a'; // output to .dta instead of .dtb
    out_file = fopen(argv[1], "w");
    
    // skip header
    fread(buffer, sizeof(char), HEADER_SIZE, dtb_file);
    
    parse_tree(dtb_file, out_file, 0, 0, 0);
    
    fclose(dtb_file);
    free(buffer);

    return 0;
}

void parse_tree(FILE* dtb_file, FILE* out_file, int depth, int num_nodes, int tree_type)
{
    char* buffer = malloc(sizeof(char) * BUFFER_SIZE);
    int node_counter = 0;
    int int_val;
    short children;
    int new_line = 0;

    if (depth > 0)
    {
        fprintf(out_file, "%c", get_open_bracket(tree_type));
    }
    while ((node_counter < num_nodes) || depth == 0)
    {
        // get next chunk descriptor, and break loop if this fails
        if (fread(buffer, sizeof(char), 4, dtb_file) == 0)
        {
            break;
        }

        if (!new_line && num_nodes > 2 && node_counter > 0)
        {
            new_line = 1;
            fprintf(out_file, "\n");
        }

        if (new_line)
        {
            print_tabs(out_file, depth);
            new_line = 0;
        }
        else if (node_counter > 0)
        {
            fprintf(out_file, " ");
        }
        
        switch (buffer[0])
        {
            case 0x00: // 32 bit int, 4 bytes
                parse_int_val(dtb_file, out_file);
                break;
            case 0x01: // 32 bit float, 4 bytes
                parse_float_val(dtb_file, out_file);
                break;
            case 0x02: // variable name, 4 bytes specify size of string that follows
                fprintf(out_file, "$");
                parse_string_val(dtb_file, out_file);
                break;
            case 0x05: // keyword (unquoted string), 4 bytes specify size of string that follows
                parse_string_val(dtb_file, out_file);
                break;
            case 0x06: // kDataUnhandled marker, 4 empty bytes follow
                fprintf(out_file, "kDataUnhandled");
                parse_string_val(dtb_file, out_file);
                break;
            case 0x07: // #ifdef directive, string, 4 bytes specify size of macro string that follows
                parse_directive(dtb_file, out_file, "#ifdef", 1);
                new_line = 1;
                break;
            case 0x08: // #else directive, 4 empty bytes follow
                parse_directive(dtb_file, out_file, "#else", 1);
                new_line = 1;
                break;
            case 0x09: // #endif directive, 4 empty bytes follow
                parse_directive(dtb_file, out_file, "#endif", 1);
                new_line = 1;
                break;
            case 0x10: // sub-tree, 6 bytes: 2 bytes = num children, 4 bytes = node id, bracketed with ()
                fread(&children, sizeof(short), 1, dtb_file);
                fread(&int_val, sizeof(int), 1, dtb_file);
                parse_tree(dtb_file, out_file, depth+1, children, 0);
                if (node_counter != num_nodes - 1)
                {
                    fprintf(out_file, "\n");
                    new_line = 1;
                }
                break;
            case 0x11: // sub-tree, 6 bytes: 2 bytes = num children, 4 bytes = node id, bracketed with {} - function calls/control structures
                fread(&children, sizeof(short), 1, dtb_file);
                fread(&int_val, sizeof(int), 1, dtb_file);
                parse_tree(dtb_file, out_file, depth+1, children, 1);
                if (node_counter != num_nodes - 1)
                {
                    fprintf(out_file, "\n");
                    new_line = 1;
                }
                break;
            case 0x12: // data string (quoted string), 4 bytes specify size of string that follows
                fprintf(out_file, "\"");
                parse_string_val(dtb_file, out_file);
                fprintf(out_file, "\"");
                break;
            case 0x13: // sub-tree, 6 bytes: 2 bytes = num children, 4 bytes = node id, bracketed with [] - unknown purpose
                fread(&children, sizeof(short), 1, dtb_file);
                fread(&int_val, sizeof(int), 1, dtb_file);
                parse_tree(dtb_file, out_file, depth+1, children, 2);
                if (node_counter != num_nodes - 1)
                {
                    fprintf(out_file, "\n");
                    new_line = 1;
                }
                break;
            case 0x20: // #define directive, 4 bytes specify size of macro string that follows
                parse_directive(dtb_file, out_file, "#define", 0);
                new_line = 0;
                break;
            case 0x21: // #include directive, 4 bytes specify size of filename string that follows
                parse_directive(dtb_file, out_file, "#include", 1);
                new_line = 1;
                break;
            case 0x22: // #merge directive, 4 bytes specify size of filename string that follows
                parse_directive(dtb_file, out_file, "#merge", 1);
                new_line = 1;
                break;
            case 0x23: // #ifndef directive, 4 bytes specify size of macro string that follows
                parse_directive(dtb_file, out_file, "#ifndef", 1);
                new_line = 1;
                break;
        }
        node_counter++;
    }
    if (depth > 0)
    {
        fprintf(out_file, "%c", get_close_bracket(tree_type));
    }

    free(buffer);
}

void print_tabs(FILE* out_file, int num_tabs)
{
    int i;
    
    for (i = 0; i < num_tabs; i++)
    {
        fprintf(out_file, "\t");
    }
}

char get_open_bracket(int tree_type)
{
    switch (tree_type)
    {
        case 1:
            return '{';
            break;
        case 2:
            return '[';
            break;
        default:
            return '(';
    }
}

char get_close_bracket(int tree_type)
{
    switch (tree_type)
    {
        case 1:
            return '}';
            break;
        case 2:
            return ']';
            break;
        default:
            return ')';
    }
}

void parse_string_val(FILE* dtb_file, FILE* out_file)
{
    int length;
    char* string_val;

    int i, j;
    int num_quotes = 0;
    char* escaped_string;

    fread(&length, sizeof(int), 1, dtb_file); // get the string length
    string_val = (char *)malloc(sizeof(char) * (length+1));
    fread(string_val, sizeof(char), length, dtb_file);
    string_val[length] = 0x00; // add null char at the end of the string

    // check for double-quotes in the string (need to be replaced with "\q")
    for (i = 0; i < length; i++)
    {
        if (string_val[i] == '"')
        {
            num_quotes++;
        }
    }
    if (num_quotes > 0)
    {
        escaped_string = (char *)malloc(sizeof(char) * (length + num_quotes + 1));
        j = 0;
        for (i = 0; i <= length; i++)
        {
            if (string_val[i] == '"')
            {
                escaped_string[j++] = '\\';
                escaped_string[j++] = 'q';
            }
            else
            {
                escaped_string[j++] = string_val[i];
            }
        }
        free(string_val);
        string_val = escaped_string;
    }

    fprintf(out_file, "%s", string_val);

    free(string_val);
}

void parse_int_val(FILE* dtb_file, FILE* out_file)
{
    int int_val;

    fread(&int_val, sizeof(int), 1, dtb_file);
    fprintf(out_file, "%d", int_val);
}

void parse_float_val(FILE* dtb_file, FILE* out_file)
{
    float float_val;

    fread(&float_val, sizeof(float), 1, dtb_file);
    fprintf(out_file, "%.2f", float_val);
}

void parse_directive(FILE* dtb_file, FILE* out_file, char* directive, int newline_after)
{
    fprintf(out_file, "\n%s ", directive);
    parse_string_val(dtb_file, out_file);
    if (newline_after)
    {
        fprintf(out_file, "\n");
    }
}
