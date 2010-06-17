
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include "xcfun_internal.h"

static bool xcfun_is_setup = false;

static void xcint_assure_setup()
{
  if (!xcfun_is_setup)
    {
      xcint_setup_functionals();
      xcfun_is_setup = true;
    }
}

void xcint_die(const char *message, int code)
{
  fprintf(stderr,"XCFun fatal error %i: ",code);
  fprintf(stderr,"%s",message);
  fprintf(stderr,"\n");
  exit(-1);
}

extern "C"
double xcfun_version(void)
{
  return 0.98;
}

extern "C"
const char *xcfun_splash(void)
{
  return "XCFun DFT library, Copyright 2009-2010 Ulf Ekstrom\n"
    "and contributors. See http://admol.org/xcfun for more information.\n"
    "This is free software; see the source code for copying conditions.\n"
    "There is ABSOLUTELY NO WARRANTY; not even for MERCHANTABILITY or\n"
    "FITNESS FOR A PARTICULAR PURPOSE. For details see the documentation.\n";
}

int xcint_input_length(int mode, int type)
{
  static int tab[XC_NR_MODES][XC_NR_TYPES] = 
    {{1,2,3},{1,2,3},{2,5,7},{2,5,7}};
  assert(mode>=0 and mode <= XC_NR_MODES);
  assert(type>=0 and type <= XC_NR_TYPES);
  return tab[mode][type];
}

int xcint_output_length(int mode, int type, int order)
{
  return taylorlen(xcint_input_length(mode,type),order);
}


void xc_functional_data::initialize()
{
  xcint_assure_setup();
  mode = XC_VARS_AB; //Use alpha/beta as default
  type = XC_LDA; // LDA is smallest type, so assume this initially
  max_order = -1;
  // Just default settings to start with
  for (int i=0;i<XC_NR_PARAMS;i++)
    parameters[i] = xcint_default(i);
  //active_functionals.construct();
}

void xc_functional_data::destroy()
{
  // active_functionals.destroy();
}

void xc_functional_data::regularize_density(double *density)
{
  //TODO: Actually do something here.
}

int xc_functional_data::get_type(void) const
{
  return type;
}
void xc_functional_data::set_mode(int mode)
{
  if (!(mode>=0 and mode < XC_NR_MODES))
    xcint_die("Invalid mode to xc_functional::set_mode()",mode);
  this->mode = mode;
  find_max_order();
}

void xc_functional_data::find_max_order(void)
{
  max_order = -1;
  while (max_order < XC_MAX_ORDER and xc_evaluator_lookup(mode,type,max_order+1))
    max_order++;
}

int xc_functional_data::get_max_order(void) const
{
  return max_order;
}


int xc_functional_data::input_length(void) const
{
  return xcint_input_length(mode,type);
}

int xc_functional_data::output_length(int order) const
{
  return taylorlen(input_length(),order);
}

int xc_functional_data::
derivative_index(const int exponents[]) const
{
  int nvar = input_length();
  int N = 0;
  for (int i=0;i<nvar;i++)
    N += exponents[i];
  int i = 0, idx = 0;
  N--;
  while (N >= 0)
    {
      idx += taylorlen(nvar-i,N);
      N -= exponents[i];
      i++;
    }
  return idx;
}


// API starts here
extern "C"
xc_functional xc_new_functional(void)
{
  xc_functional_data *p = (xc_functional_data *)malloc(sizeof*p);
  p->initialize();
  return p;
}

extern "C"
void xc_free_functional(xc_functional fun)
{
  fun->destroy();
  free(fun);
}

void xc_regularize_density(xc_functional fun, double *density)
{
  fun->regularize_density(density);
}

void xc_eval(xc_functional fun, int order, int nr_points,
	     const double *density, double *result)
{
  evaluator ev = xc_evaluator_lookup(fun->mode,fun->type,order);
  if (!ev)
    {
      fprintf(stderr,"XCFun error in eval()\n");
      fprintf(stderr,"mode: %i\n",fun->mode);
      fprintf(stderr,"type: %i\n",fun->type);
      xcint_die("eval(): Functional not available for order",order);
    }
  int inlen = xc_input_length(fun); // TODO: cache this in fun
  int outlen = xc_output_length(fun,order);
  for (int i=0;i<nr_points;i++)
    ev(*fun,result+i*outlen,density+i*inlen);
}

int xc_get_type(xc_functional fun)
{
  return fun->get_type();
}

int xc_max_order(xc_functional fun)
{
  return fun->get_max_order();
}

int xc_input_length(xc_functional fun)
{
  return fun->input_length();
}

int xc_output_length(xc_functional fun, int order)
{
  return fun->output_length(order);
}

int xc_derivative_index(xc_functional fun, const int derivative[])
{
  return fun->derivative_index(derivative);
}

void xc_set_mode(xc_functional fun, int mode)
{
  fun->set_mode(mode);
}

void xc_set_param(xc_functional fun, int param, double value)
{
  if (param < 0 or param >= XC_NR_PARAMS)
      xcint_die("Invalid parameter in xc_set_param()",param);
  fun->parameters[param] = value;
  if (xc_is_functional(param) and value != 0)
    {
      if (xcint_functional(param)->m_type > fun->type)
	fun->type = xcint_functional(param)->m_type;
      fun->find_max_order();
    } 
}

double xc_get_param(xc_functional fun, int param)
{
  if (param < 0 or param >= XC_NR_PARAMS)
      xcint_die("Invalid parameter in xc_get_param()",param);
  return fun->parameters[param];
}

extern "C"
int xc_is_functional(int param)
{
  return xcint_functional(param) != 0;
}


static int run_tests(functional *fun)
{
  if (fun->test_mode == -1)
    return -1;
  xc_functional xf = xc_new_functional();
  xc_set_mode(xf,fun->test_mode);
  xc_set_param(xf,fun->m_name,1.0);
  int n = xc_output_length(xf, fun->test_order);
  double *out = new double[n];
  double *reference = fun->test_output;
  xc_eval(xf,fun->test_order,1,fun->test_input,out);

  int nerr = 0;
  for (int i=0;i<n;i++)
    if (fabs(out[i] - fun->test_output[i]) > 
	fabs(fun->test_output[i]*fun->test_threshold))
      nerr++;

  if (nerr > 0)
    {
      fprintf(stderr,"Error detected in functional %s with tolerance %g:\n",
	      xc_name(fun->m_name),fun->test_threshold);
      fprintf(stderr,"Abs.Error \tComputed              Reference\n");
      for (int i=0;i<n;i++)
	{
	  fprintf(stderr,"%.1e",fabs(out[i]-reference[i]));
	  fprintf(stderr,"    %+.16e \t%+.16e",out[i],reference[i]);
	  if (fabs(out[i] - reference[i]) > 
	      fabs(reference[i]*fun->test_threshold))
	     fprintf(stderr," *");
	  fprintf(stderr,"\n");
	}
    }
  else
    {
      printf("%s ok\n",xc_name(fun->m_name));
    }
  xc_free_functional(xf);
  return nerr;
}

int xcfun_test(void)
{
  int nr_failed = 0;
  int nr_run = 0;
  xcint_assure_setup();
  for (int i=0;i<XC_NR_PARAMS;i++)
    {
      functional *f = xcint_functional(i);
      if (f)
	{
	  nr_run++;
	  int res = run_tests(f);
	  if (res != 0 and res != -1)
	    nr_failed++;
	}
    }
  printf("Nr tests run: %i\n",nr_run);
  return nr_failed;
}