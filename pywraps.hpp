//--------------------------------------------------------------------------
// Functions taken from code written by Elias Bachaalany for the IDAPython project
//
//--------------------------------------------------------------------------

//--------------------------------------------------------------------------
#ifdef __EA64__
  #define PY_FMT64  "K"
  #define PY_SFMT64 "L"
#else
  #define PY_FMT64  "k"
  #define PY_SFMT64 "l"
#endif

//------------------------------------------------------------------------
// Constants used by the pyvar_to_idcvar and idcvar_to_pyvar functions
#define CIP_FAILED      -1 // Conversion error
#define CIP_IMMUTABLE    0 // Immutable object passed. Will not update the object but no error occured
#define CIP_OK           1 // Success
#define CIP_OK_NODECREF  2 // Success but do not decrement its reference

//-------------------------------------------------------------------------
// Parses a Python object as a long or long long
bool PyW_GetNumber(PyObject *py_var, uint64 *num, bool *is_64 = NULL)
{
  if ( !(PyInt_CheckExact(py_var) || PyLong_CheckExact(py_var)) )
    return false;

  // Can we convert to C long?
  long l = PyInt_AsLong(py_var);
  if ( !PyErr_Occurred() )
  {
    if ( num != NULL )
      *num = uint64(l);
    if ( is_64 != NULL )
      *is_64 = false;
    return true;
  }

  // Clear last error
  PyErr_Clear();

  // Can be fit into a C unsigned long?
  unsigned long ul = PyLong_AsUnsignedLong(py_var);
  if ( !PyErr_Occurred() )
  {
    if ( num != NULL )
      *num = uint64(ul);
    if ( is_64 != NULL )
      *is_64 = false;
    return true;
  }
  PyErr_Clear();

  // Try to parse as int64
  PY_LONG_LONG ll = PyLong_AsLongLong(py_var);
  if ( !PyErr_Occurred() )
  {
    if ( num != NULL )
      *num = uint64(ll);
    if ( is_64 != NULL )
      *is_64 = true;
    return true;
  }
  PyErr_Clear();

  // Try to parse as uint64
  unsigned PY_LONG_LONG ull = PyLong_AsUnsignedLongLong(py_var);
  PyObject *err = PyErr_Occurred();
  if ( err == NULL )
  {
    if ( num != NULL )
      *num = uint64(ull);
    if ( is_64 != NULL )
      *is_64 = true;
    return true;
  }
  // Negative number? _And_ it with uint64(-1)
  bool ok = false;
  if ( err == PyExc_TypeError )
  {
    PyObject *py_mask = Py_BuildValue("K", 0xFFFFFFFFFFFFFFFFull);
    PyObject *py_num = PyNumber_And(py_var, py_mask);
    if ( py_num != NULL && py_mask != NULL )
    {
      PyErr_Clear();
      ull = PyLong_AsUnsignedLongLong(py_num);
      if ( !PyErr_Occurred() )
      {
        if ( num != NULL )
          *num = uint64(ull);
        if ( is_64 != NULL )
          *is_64 = true;
        ok = true;
      }
    }
    Py_XDECREF(py_num);
    Py_XDECREF(py_mask);
  }
  PyErr_Clear();
  return ok;
}

//-------------------------------------------------------------------------
// Checks if a given object is of sequence type
bool PyW_IsSequenceType(PyObject *obj)
{
  if ( !PySequence_Check(obj) )
    return false;

  Py_ssize_t sz = PySequence_Size(obj);
  if ( sz == -1 || PyErr_Occurred() != NULL )
  {
    PyErr_Clear();
    return false;
  }
  return true;
}

//--------------------------------------------------------------------------
void PyW_RunPyFile(const char *fn)
{
  char *v_fn = qstrdup(fn);
  PyObject *py_fp = PyFile_FromString(v_fn, "r");
  FILE *fp = PyFile_AsFile(py_fp);
  PyRun_SimpleFile(fp, v_fn);
  qfree(v_fn);
}

//------------------------------------------------------------------------
// Tries to import a module and clears the exception on failure
PyObject *PyW_TryImportModule(const char *name)
{
  PyObject *result = PyImport_ImportModule(name);
  if (result != NULL)
    return result;

  if (PyErr_Occurred())
    PyErr_Clear();

  return NULL;
}

//------------------------------------------------------------------------
// Returns an attribute or NULL
// No errors will be set if the attribute did not exist
PyObject *PyW_TryGetAttrString(PyObject *py_obj, const char *attr)
{
  if ( !PyObject_HasAttrString(py_obj, attr) )
    return NULL;
  else
    return PyObject_GetAttrString(py_obj, attr);
}

//-------------------------------------------------------------------------
Py_ssize_t pyvar_walk_list(
  PyObject *py_list,
  int (*cb)(PyObject *py_item, Py_ssize_t index, void *ud),
  void *ud)
{
  if ( !PyList_CheckExact(py_list) && !PyW_IsSequenceType(py_list) )
    return CIP_FAILED;

  bool is_seq = !PyList_CheckExact(py_list);
  Py_ssize_t size = is_seq ? PySequence_Size(py_list) : PyList_Size(py_list);

  if ( cb == NULL )
    return size;

  Py_ssize_t i;
  for ( i=0; i<size; i++ )
  {
    // Get the item
    PyObject *py_item = is_seq ? PySequence_GetItem(py_list, i) : PyList_GetItem(py_list, i);
    if ( py_item == NULL )
      break;

    int r = cb(py_item, i, ud);

    // Decrement reference (if needed)
    if ( r != CIP_OK_NODECREF && is_seq )
      Py_DECREF(py_item); // Only sequences require us to decrement the reference
    if ( r < CIP_OK )
      break;
  }
  return i;
}

//---------------------------------------------------------------------------
PyObject *PyW_IntVecToPyList(const intvec_t &intvec)
{
  size_t c = intvec.size();
  PyObject *py_list = PyList_New(c);

  for ( size_t i=0; i<c; i++ )
    PyList_SetItem(py_list, i, PyInt_FromLong(intvec[i]));

  return py_list;
}

//---------------------------------------------------------------------------
static int pylist_to_intvec_cb(
  PyObject *py_item,
  Py_ssize_t /*index*/,
  void *ud)
{
  intvec_t &intvec = *(intvec_t *)ud;
  uint64 num;
  if (!PyW_GetNumber(py_item, &num))
    num = 0;

  intvec.push_back(int(num));
  return CIP_OK;
}

//---------------------------------------------------------------------------
bool PyW_PyListToIntVec(PyObject *py_list, intvec_t &intvec)
{
  intvec.clear();
  return pyvar_walk_list(py_list, pylist_to_intvec_cb, &intvec) != CIP_FAILED;
}

//--------------------------------------------------------------------------
bool PyW_PyListListToIntVecVec(PyObject *py_list, int_2dvec_t &result)
{
  class cvt_t
  {
  private:
    int_2dvec_t &similar;
  public:
    cvt_t(int_2dvec_t &similar): similar(similar)
    {
    }

    static int cb(PyObject *py_item, Py_ssize_t index, void *ud)
    {
      cvt_t *_this = (cvt_t *)ud;

      // Declare an empty list
      intvec_t lst;

      // Push it immediately to the vector (to avoid double copies)
      _this->similar.push_back(lst);

      // Now convert the inner list directly into the vector
      PyW_PyListToIntVec(py_item, _this->similar.back());

      return CIP_OK;
    }
  };
  cvt_t cvt(result);
  return pyvar_walk_list(py_list, cvt_t::cb, &cvt) != CIP_FAILED;
}

//--------------------------------------------------------------------------
bool PyW_PyListListToIntVecVecVec(PyObject *py_list, int_3dvec_t &result)
{
  class cvt_t
  {
  private:
    int_3dvec_t &v;
  public:
    cvt_t(int_3dvec_t &v): v(v)
    {
    }

    static int cb(
        PyObject *py_item, 
        Py_ssize_t index, 
        void *ud)
    {
      cvt_t *_this = (cvt_t *)ud;

      // Declare an empty 2D list
      int_2dvec_t lst;

      // Push it immediately to the vector (to avoid double copies)
      _this->v.push_back(lst);

      // Now convert the inner list directly into the vector
      PyW_PyListListToIntVecVec(py_item, _this->v.back());

      return CIP_OK;
    }
  };
  cvt_t cvt(result);
  return pyvar_walk_list(py_list, cvt_t::cb, &cvt) != CIP_FAILED;
}