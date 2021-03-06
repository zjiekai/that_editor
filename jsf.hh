/* Ad-hoc programming editor for DOSBox -- (C) 2011-03-08 Joel Yliluoma */
#include "langdefs.hh"
#include <string.h>

#if defined(__cplusplus) && __cplusplus >= 199700L
# include <algorithm>
#endif

#include "vec_c.hh" // For the implementation of buffer

#if defined(__cplusplus) && __cplusplus >= 199700L
template<class DerivedClass>
#endif
class JSF
#if defined(__cplusplus) && __cplusplus >= 199700L
          : public DerivedClass
#endif
{
public:
    JSF() : states(nullptr)
    {
    }
    ~JSF()
    {
        //Clear();
    }
    void Parse(const char* fn)
    {
        FILE* fp = fopen(fn, "rb");
        if(!fp) { perror(fn); return; }
        Parse(fp);
        fclose(fp);
    }
    void Parse(FILE* fp)
    {
        char Buf[512]={0};
        //fprintf(stdout, "Parsing syntax file... "); fflush(stdout);
        TabType colortable;
        bool colors_sorted = false;
        //Clear();
        while(fgets(Buf, sizeof(Buf), fp))
        {
            cleanup(Buf);
            if(Buf[0] == '=')
            {
                colors_sorted = false;
                ParseColorDeclaration(Buf+1, colortable);
            }
            else if(Buf[0] == ':')
            {
                if(!colors_sorted)
                {
                    /* Sort the color table when the first state is encountered */
                    sort(colortable);
                    colors_sorted = true;
                }
                ParseStateStart(Buf+1, colortable);
            }
            else if(Buf[0] == ' ' || Buf[0] == '\t')
                ParseStateLine(Buf, fp);
        }
        //fprintf(stdout, "Binding... "); fflush(stdout);
        BindStates();
        //fprintf(stdout, "Done\n"); fflush(stdout);
        for(unsigned n=0; n<colortable.size(); ++n) free(colortable[n].token);
    }
    struct state;
    struct ApplyState
    {
        /* std::vector<unsigned char> */
        CharVecType buffer;
        bool buffering;
        int recolor, markbegin, markend;
        bool recolormark, noeat;
        unsigned char c;
        state* s;
    };
    void ApplyInit(ApplyState& state)
    {
        state.buffer.clear();
        state.buffering = state.noeat = false;
        state.recolor = state.markbegin = state.markend = 0;
        state.c = '?';
        state.s = states;
    }
#if defined(__cplusplus) && __cplusplus >= 199700L
    void Apply( ApplyState& state )
#else
    struct Applier
    {
        virtual cdecl int Get(void) = 0;
        virtual cdecl void Recolor(register unsigned distance, register unsigned n, register EditorCharType attr) = 0;
    };
    void Apply( ApplyState& state, Applier& app)
#endif
    {
#if defined(__cplusplus) && __cplusplus >= 199700L
        DerivedClass& app = *this;
#endif
        for(;;)
        {
            /*fprintf(stdout, "[State %s]", state.s->name);*/
            if(state.noeat)
            {
                state.noeat = false;
                if(!state.recolor) state.recolor = 1;
            }
            else
            {
                int ch = app.Get();
                if(ch < 0) break;
                state.c       = ch;
                state.recolor += 1;
                ++state.markbegin;
                ++state.markend;
            }
            if(state.recolor)
            {
                app.Recolor(0, state.recolor, state.s->attr);
            }
            if(state.recolormark)
            {
                // markbegin & markend say how many characters AGO it was marked
                app.Recolor(state.markend+1, state.markbegin - state.markend, state.s->attr);
            }

            option *o = state.s->options[state.c];
            state.recolor     = o->recolor;
            state.recolormark = o->recolormark;
            state.noeat       = o->noeat;
            state.s           = o->state;
            if(o->strings)
            {
                const char* k = (const char*) &state.buffer[0];
                unsigned    n = state.buffer.size();
                struct state* ns = o->strings==1
                        ? findstate(o->stringtable, k, n)
                        : findstate_i(o->stringtable, k, n);
                /*fprintf(stdout, "Tried '%.*s' for %p (%s)\n",
                    n,k, ns, ns->name);*/
                if(ns)
                {
                    state.s = ns;
                    state.recolor = state.buffer.size()+1;
                }
                state.buffer.clear();
                state.buffering = false;
            }
            else if(state.buffering && !state.noeat)
                state.buffer.push_back(state.c);
            if(o->buffer)
                { state.buffering = true;
                  state.buffer.assign(&state.c, &state.c + 1); }
            if(o->mark)    { state.markbegin = 0; }
            if(o->markend) { state.markend   = 0; }
        }
    }
private:
    struct option;
    struct state
    {
        state*         next;
        char*          name;
        EditorCharType attr;
        option* options[256];
        // Note: cleared using memset
    }* states;
    struct table_item
    {
        char*  token;
        union
        {
            struct state* state;
            char*  state_name;
        };

#if defined(__cplusplus) && __cplusplus >= 199700L
        inline table_item()                    { token=nullptr; state=nullptr; }
        inline table_item(const table_item& b) { token=b.token; state=b.state; }
        inline ~table_item()                   { }
        inline table_item& operator=(const table_item& b) { token=b.token; state=b.state; return *this; }
#else
        inline void Construct() { token=nullptr; state=nullptr; }
        inline void Construct(const table_item& b) { token=b.token; state=b.state; }
        inline void Destruct()  { }
#endif
#if defined(__cplusplus) && __cplusplus >= 201100L
        inline void Construct(table_item&& b)        { token=b.token; state=b.state; b.token = nullptr; b.state = nullptr; }
        inline table_item(table_item&& b)            { Construct(std::move(b)); }
        inline table_item& operator=(table_item&& b) { Construct(std::move(b)); return *this; }
#else
        inline void swap(table_item& b)
        {
            register char* t;
            t = token;      token     =b.token;      b.token     =t;
            t = state_name; state_name=b.state_name; b.state_name=t;
        }
#endif
    };
    /* std::vector<table_item> without STL, for Borland C++ */
    #define UsePlacementNew
    #define o(x) x(table_item,TabType)
    #include "vecbase.hh"
    #undef o
    #undef UsePlacementNew

    struct option
    {
        TabType stringtable;
        union
        {
            struct state* state;
            char*         state_name;
        };
        unsigned char recolor;
        bool     noeat:  1;
        bool     buffer: 1;
        unsigned strings:2; // 0=no strings, 1=strings, 2=istrings
        bool     name_mapped:1; // whether state(1) or state_name(0) is valid
        bool     mark:1, markend:1, recolormark:1;

        option(): stringtable(),state(nullptr),recolor(0),noeat(0),buffer(0),strings(0),name_mapped(0),mark(0),markend(0),recolormark(0)
        {
        }
    };
    inline static unsigned long ParseColorDeclaration(char* line)
    {
        unsigned char fg256 = 0;
        unsigned char bg256 = 0;
        unsigned char flags = 0x00; // underline=1 dim=2 italic=4 bold=8 inverse=16 blink=32
        for(;;)
        {
            while(*line==' '||*line=='\t') ++line;
            if(!*line) break;
            {char* line_end = nullptr;
            {unsigned char val = strtol(line, &line_end, 16);
            if(line_end >= line+2) // Two-digit hex?
            {
                line     = line_end;
                fg256    = val & 0xF;
                bg256    = val; bg256 >>= 4;
                continue;
            }}
            if(line[1] == 'g' && line[2] == '_' && line[3] >= '0' && line[3] <= '9')
            {
                unsigned base = (line[5] >= '0' && line[5] <= '5') ? (16+(6<<8)) : (232+(10<<8));
                unsigned char val = (unsigned char)(base) + strtol(line+3, &line_end, base>>8u);
                switch(line[0]) { case 'b': bg256 = val; break;
                                  case 'f': fg256 = val; break; }
                line = line_end; continue;
            }}
            /* Words: black blue cyan green red yellow magenta white
             *        BLACK BLUE CYAN GREEN RED YELLOW MAGENTA WHITE
             *        bg_black bg_blue bg_cyan bg_green bg_red bg_yellow bg_magenta bg_white
             *        BG_BLACK BG_BLUE BG_CYAN BG_GREEN BG_RED BG_YELLOW BG_MAGENTA BG_WHITE
             *        underline=1 dim=2 italic=4 bold=8 inverse=16 blink=32
             *
             * This hash has been generated using find_jsf_formula.cc.
             * As long as it differentiates the *known* words properly
             * it does not matter what it does to unknown words.
             */
            unsigned short c=0, i=0;
//Good: 90 28   distance = 46  mod=46  div=26
            while(*line && *line != ' ' && *line != '\t') { c += 90u*(unsigned char)*line + i; i+=28; ++line; }
            unsigned char code = ((c + 22u) / 26u) % 46u;

#ifdef ATTRIBUTE_CODES_IN_ANSI_ORDER
            static const signed char actions[46] = { 10,29,2,4,31,22,27,23,11,36,15,28,7,6,25,-1,17,-1,24,12,16,30,-1,8,35,0,9,19,-1,3,14,20,21,33,32,34,1,13,-1,-1,5,26,-1,-1,18,37};
#endif
#ifdef ATTRIBUTE_CODES_IN_VGA_ORDER
            static const signed char actions[46] = { 10,30,2,1,31,19,29,23,13,36,15,25,7,3,28,-1,20,-1,24,9,16,27,-1,8,35,0,12,21,-1,5,11,17,22,33,32,34,4,14,-1,-1,6,26,-1,-1,18,37};
#endif
            /*if(code >= 0 && code <= 45)*/ code = actions[code - 0]; // cekcpaka
            switch(code >> 4) { case 0: fg256 = code&15; break;
                                case 1: bg256 = code&15; break;
                                default:flags |= 1u << (code&15); }
        }
        // Type is unsigned long to avoid compiler warnings from pointer cast
        unsigned long attr = ComposeEditorChar('\0', fg256, bg256, flags);
        if(!attr) attr |= 0x80000000ul; // set 1 dummy bit in order to differentiate from nuls
        return attr;
    }
    inline void ParseColorDeclaration(char* line, TabType& colortable)
    {
        while(*line==' '||*line=='\t') ++line;
        char* namebegin = line;
        while(*line && *line != ' ' && *line!='\t') ++line;
        char* nameend = line;
        unsigned long attr = ParseColorDeclaration(line);
        *nameend = '\0';
        table_item tmp;
        tmp.token = strdup(namebegin);
        if(!tmp.token) fprintf(stdout, "strdup: failed to allocate string for %s\n", namebegin);
        tmp.state = (struct state *)attr;
        colortable.push_back(tmp);
    }
    inline void ParseStateStart(char* line, const TabType& colortable)
    {
        while(*line==' '||*line=='\t') ++line;
        char* namebegin = line;
        while(*line && *line != ' ' && *line!='\t') ++line;
        char* nameend = line;
        while(*line==' '||*line=='\t') ++line;
        *nameend = '\0';
        struct state* s = new state;
        if(!s) fprintf(stdout, "failed to allocate new jsf state\n");
        memset(s, 0, sizeof(*s));
        s->name = strdup(namebegin);
        if(!s->name)
        {
            fprintf(stdout, "strdup: failed to allocate string for %s\n", namebegin);
            s->attr = MakeJSFerrorColor('\0');
        }
        else
        {
            state* c = findstate(colortable, line);
            // The value in the table is a pointer type, but it actually is a color code (integer).
            if(!c)
            {
                fprintf(stdout,"Unknown color: '%s'\n", line);
                s->attr = MakeJSFerrorColor('\0');
            }
            else
            {
                s->attr = (EditorCharType)(unsigned long)c;
            }
        }
        s->next = states;
        states = s;
    }
    inline void ParseStateLine(char* line, FILE* fp)
    {
        option* o = new option;
        if(!o) fprintf(stdout, "failed to allocate new jsf option\n");
        while(*line == ' ' || *line == '\t') ++line;
        if(*line == '*')
        {
            for(unsigned a=0; a<256; ++a)
                states->options[a] = o;
            ++line;
        }
        else if(*line == '"')
        {
            for(++line; *line != '\0' && *line != '"'; ++line)
            {
                if(*line == '\\')
                    switch(*++line)
                    {
                        case 't': *line = '\t'; break;
                        case 'n': *line = '\n'; break;
                        case 'v': *line = '\v'; break;
                        case 'b': *line = '\b'; break;
                    }
                unsigned char first = *line;
                if(line[1] == '-' && line[2] != '"')
                {
                    line += 2;
                    if(*line == '\\')
                        switch(*++line)
                        {
                            case 't': *line = '\t'; break;
                            case 'n': *line = '\n'; break;
                            case 'v': *line = '\v'; break;
                            case 'b': *line = '\b'; break;
                        }
                    do states->options[first] = o;
                    while(first++ != (unsigned char)*line);
                }
                else
                    states->options[first] = o;
            }
            if(*line == '"') ++line;
        }
        while(*line == ' ' || *line == '\t') ++line;
        char* namebegin = line;
        while(*line && *line != ' ' && *line!='\t') ++line;
        char* nameend   = line;
        while(*line == ' ' || *line == '\t') ++line;
        *nameend = '\0';
        o->state_name  = strdup(namebegin);
        if(!o->state_name) fprintf(stdout, "strdup: failed to allocate string for %s\n", namebegin);
        o->name_mapped = false;
        /*fprintf(stdout, "'%s' for these: ", o->state_name);
        for(unsigned c=0; c<256; ++c)
            if(states->options[c] == o)
                fprintf(stdout, "%c", c);
        fprintf(stdout, "\n");*/

        while(*line != '\0')
        {
            char* opt_begin = line;
            while(*line && *line != ' ' && *line!='\t') ++line;
            char* opt_end   = line;
            while(*line == ' ' || *line == '\t') ++line;
            *opt_end = '\0';

            /* Words: noeat buffer markend mark strings istrings recolormark recolor=
             * This hash has been generated using jsf-keyword-hash2.php.
             */
            register unsigned char n=2;
            {for(register unsigned char v=0;;)
            {
                char c = *opt_begin++;
                n += (c ^ v) + 6;
                v += 2;
                if(c == '=' || c == '\0') break;
            }}
            switch((n >> 3u) & 7)
            {
                case 0: o->recolormark = true; break; // recolormark
                case 1: o->noeat       = true; break; // noeat
                case 3: o->mark        = true; break; // mark
                case 4: o->strings     = 1;    break; // strings
                case 5: o->markend     = true; break; // markend
                case 6: o->strings     = 2;    break; // istrings
                case 7: o->buffer      = true; break; // buffer
                //default:fprintf(stdout,"Unknown keyword '%s' in '%s'\n", opt_begin-1, namebegin); break;
                case 2: int r = atoi(opt_begin);// recolor=
                        if(r < 0) r = -r;
                        o->recolor = r;
                        break;
            }
        }
        if(o->strings)
        {
            for(;;)
            {
                char Buf[512]={0};
                if(!fgets(Buf, sizeof(Buf), fp)) break;
                cleanup(Buf);
                line = Buf;
                while(*line == ' ' || *line == '\t') ++line;
                if(strcmp(line, "done") == 0) break;
                if(*line == '"') ++line;

                char* key_begin = line = strdup(line);
                if(!key_begin) fprintf(stdout, "strdup: failed to allocate string for %s\n", line);
                while(*line != '"' && *line != '\0') ++line;
                char* key_end   = line;
                if(*line == '"') ++line;
                while(*line == ' ' || *line == '\t') ++line;
                *key_end++   = '\0';

                char* value_begin = line;
                while(*line != '\0') ++line;
                /*unsigned char* value_end   = (unsigned char*) line;
                *value_end++ = '\0';*/
                if(*key_begin && *value_begin)
                {
                    table_item item;
                    item.token      = key_begin;
                    item.state_name = value_begin;
                    //fprintf(stdout, "String-table push '%s' '%s'\n", key_begin,value_begin);
                    o->stringtable.push_back(item);
                }
            }
            sort(o->stringtable);
        }
    }
    // Removes comments and trailing space from the buffer
    static void cleanup(char* Buf)
    {
        char quote=0, *begin = Buf, *end = strchr(Buf, '\0');
        for(; *begin; ++begin)
        {
            if(*begin == '#' && !quote)
                { end=begin; *begin='\0'; break; }
            if(*begin == '"') quote=!quote;
            else if(*begin == '\\') ++begin;
        }
        while(end > Buf &&
            (end[-1] == '\r'
          || end[-1] == '\n'
          || end[-1] == ' '
          || end[-1] == '\t')) --end;
        *end = '\0';
    }
    // Search given table for the given string.
    // Is used by BindStates() for finding states for binding,
    // but also used by Apply for searching a string table
    // (i.e. used when coloring reserved words).
    static state* findstate(const TabType& table, const char* s, register unsigned n=0)
    {
        if(!n) n = strlen(s);
        unsigned begin = 0, end = table.size();
        while(begin < end)
        {
            unsigned half = (end-begin) >> 1;
            const table_item& m = table[begin + half];
            register int c = strncmp(m.token, s, n);
            if(c == 0)
            {
                if(m.token[n] == '\0') return m.state;
                c = m.token[n];
            }
            if(c < 0) begin += half+1;
            else      end = begin+half;
        }
        return 0;
    }
    // Case-ignorant version
    static state* findstate_i(const TabType& table, const char* s, register unsigned n=0)
    {
        if(!n) n = strlen(s);
        unsigned begin = 0, end = table.size();
        while(begin < end)
        {
            unsigned half = (end-begin) >> 1;
            const table_item& m = table[begin + half];
            register int c = strnicmp(m.token, s, n);
            if(c == 0)
            {
                if(m.token[n] == '\0') return m.state;
                c = m.token[n];
            }
            if(c < 0) begin += half+1;
            else      end = begin+half;
        }
        return 0;
    }

    // Converted state-names into pointers to state structures for fast access
    void Remap(option*& o, TabType& state_cache, unsigned a, const char* statename)
    {
        if( ! o->name_mapped)
        {
            char* name = o->state_name;
            o->state = findstate( state_cache, name );
            if(!o->state)
            {
                fprintf(stdout, "Failed to find state called '%s' for index %u/256 in '%s'\n", name, a, statename);
            }
            o->name_mapped = true;
            for(
#if defined(__cplusplus) && __cplusplus >= 199700L
                typename
#endif
                TabType::iterator e = o->stringtable.end(),
                t = o->stringtable.begin();
                t != e;
                ++t)
            {
                char* name2 = t->state_name;
                t->state = findstate( state_cache, name2 );
                if(!t->state)
                {
                    fprintf(stdout, "Failed to find state called '%s' for string table in target '%s' for '%s'\n", name2, name, statename);
                }
                // free(name2); - was not separately allocated
            }
            free(name);
        }
    }
    void BindStates()
    {
        TabType state_cache;
        {for(state* s = states; s; s = s->next)
        {
            table_item tmp;
            tmp.token = s->name;
            tmp.state = s;
            state_cache.push_back(tmp);
        }}
        sort(state_cache);

        // Translate state names to state pointers.
        for(;;)
        {
            for(unsigned a=0; a<256; ++a)
            {
            //tail:;
                option*& o = states->options[a];
                if(!o)
                {
                    fprintf(stdout, "In state '%s', character state %u/256 not specified\n", states->name, a);
                    continue;
                }
                Remap(o, state_cache, a, states->name);
                while(o->noeat && o->recolor <= 1 && !o->buffer && !o->strings && !o->mark && !o->markend && !o->recolormark)
                {
                    EditorCharType orig_attr = states->attr;
                    EditorCharType new_attr  = o->state->attr;
                    int had_recolor          = o->recolor > 0;

                    o = o->state->options[a];
                    Remap(o, state_cache, a, o->state->name);

                    if(o->state->options[a]->recolor < 1 && (had_recolor || new_attr != orig_attr))
                    {
                        o->state->options[a]->recolor = 1;
                    }
                }
            }
            if(!states->next) break;
            // Get the first-inserted state (last in chain) as starting-point.
            states = states->next;
        }
    }

    #if !(defined(__cplusplus) && __cplusplus >= 201100L)
    static int TableItemCompareForSort(const void * a, const void * b)
    {
        table_item * aa = (table_item *)a;
        table_item * bb = (table_item *)b;
        return strcmp(aa->token, bb->token);
    }
    #endif
    static inline void sort(TabType& tab)
    {
        /*
        // Sort the table using insertion sort
        unsigned b = tab.size();
        for(unsigned i, j=1; j<b; ++j)
        {
            table_item k = tab[j];
            for(i=j; i>=1 && strcmp( k.token, tab[i-1].token ) > 0; ++i)
                tab[i] = tab[i-1];
            tab[i] = k;
        }
        */
        #if defined(__cplusplus) && __cplusplus >= 201100L
        std::sort(tab.begin(), tab.end(), [&](table_item& a, table_item& b)
                                          { return strcmp(a.token, b.token) < 0; });
        #else
        qsort(&tab[0], tab.size(), sizeof(tab[0]), TableItemCompareForSort);
        #endif
    }
    void Clear()
    {
        TabType del_list;
        // States form a linked list; it is easy to go through and delete.
        // However, options have to be garbage-collected. We use "strings=3"
        // as a flag indicating the option has been already marked for deletion.
        for(state* s = states; s; )
        {
            if(s->name) { free(s->name); s->name = nullptr; }

            option** o = s->options;
            for(unsigned n=0; n<256; ++n)
            {
                register option* opt = *o++;
                if(!opt || opt->strings==3) continue;
                opt->strings = 3;
                table_item i;
                i.state = (state*)opt; del_list.push_back(i);
            }

            state* tmp = s;
            s = s->next;
            delete tmp;
        }
        for(unsigned b=del_list.size(); b-- > 0; )
        {
            option* o = (option*) del_list[b].state;
            TabType& str = o->stringtable;
            for(unsigned c=str.size(); c-- > 0; )
                if(str[c].token)
                    free(str[c].token);
            delete o;
        }
    }
};
