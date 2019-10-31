struct param_info
{
  const char *enum_value;

  const char *option;


  int default_value;


  int min_value;


  int max_value;


  const char *help;


  const char **value_names;
};

#define NULL 0
#define INT_MAX 0

static const param_info params[] = {
#define DEFPARAM(ENUM, OPTION, HELP, DEFAULT, MIN, MAX) \
  { #ENUM, OPTION, DEFAULT, MIN, MAX, HELP, NULL },
#include "gcc/params.def"
#undef DEFPARAM
#undef DEFPARAMENUM5
  { NULL, NULL, 0, 0, 0, NULL, NULL }
};

int main()
{
  int i= 0;
  while (params[i].enum_value != NULL)
    {
      __builtin_printf ("%s;%s;%d;%d;%d;%s\n", params[i].enum_value, params[i].option,
			params[i].default_value, params[i].min_value,
			params[i].max_value, params[i].help);
      i++;
    }
}
