/*--------------------------------------------------------------------------
History
--------

11/07/2013 - eliasb             - Initial version
04/15/2014 - eliasb             - Check the result of PyAnalyze() before converting the result to C structs
--------------------------------------------------------------------------*/

#include "pybbmatcher.h"
#include "pywraps.hpp"

//--------------------------------------------------------------------------
// Consts
const char STR_PY_MATCH_MODULE[] = "bb_match";

//------------------------------------------------------------------------
// Helper function to get globals for the __main__ module
// Note: The references are borrowed. No need to free them.
static PyObject *GetMainGlobals()
{
    PyObject *module = PyImport_AddModule("__main__");
    return module == NULL ? NULL : PyModule_GetDict(module);
}

//--------------------------------------------------------------------------
const char *PyBBMatcher::call_init_file()
{
    PYW_GIL_GET;
    if (!Py_IsInitialized())
        return "Python is required to run this plugin";

    static bool init_file_executed = false;

    if (init_file_executed)
        return NULL;

    PyW_RunPyFile(init_script);
    if (PyErr_Occurred())
        return "Could not run the init script!";

    init_file_executed = true;
    return NULL;
}

//--------------------------------------------------------------------------
const char *PyBBMatcher::init()
{
    PYW_GIL_GET;
    const char *err = call_init_file();
    if (err != NULL)
        return err;

    py_matcher_module = PyW_TryImportModule(STR_PY_MATCH_MODULE);
    if (py_matcher_module == NULL)
        return "BBMatch module is not present";

    //NOTE: To create an object instance, get a reference to the class
    //      then call PyObject_CallFunctionObjArgs(py_cls, NULL)
    py_instref = PyW_TryGetAttrString(py_matcher_module, "bbMatcher");
    if (py_instref == NULL)
        return "BBMatcher instance not present";

    py_meth_find_similar = PyW_TryGetAttrString(py_instref, "FindSimilar");
    py_meth_save_state = PyW_TryGetAttrString(py_instref, "SaveState");
    py_meth_load_state = PyW_TryGetAttrString(py_instref, "LoadState");
    py_meth_analyze = PyW_TryGetAttrString(py_instref, "Analyze");

    if (   py_meth_find_similar == NULL
        || py_meth_save_state == NULL
        || py_meth_load_state == NULL
        || py_meth_analyze == NULL)
    {
        return "Failed to find one or more needed methods";
    }

    return NULL;
}

//--------------------------------------------------------------------------
void PyBBMatcher::deinit()
{
    if (py_matcher_module != NULL)
    {
        Py_DECREF(py_matcher_module);
        py_matcher_module = NULL;
    }

    if (py_instref != NULL)
    {
        Py_DECREF(py_instref);
        py_instref = NULL;
    }

    if (py_meth_find_similar != NULL)
    {
        Py_DECREF(py_meth_find_similar);
        py_meth_find_similar = NULL;
    }

    if (py_meth_save_state != NULL)
    {
        Py_DECREF(py_meth_save_state);
        py_meth_save_state = NULL;
    }

    if (py_meth_load_state != NULL)
    {
        Py_DECREF(py_meth_load_state);
        py_meth_load_state = NULL;
    }

    if (py_meth_analyze != NULL)
    {
        Py_DECREF(py_meth_analyze);
        py_meth_analyze = NULL;
    }
}

//--------------------------------------------------------------------------
void PyBBMatcher::Analyze(ea_t func_addr, int_3dvec_t &result)
{
    PYW_GIL_GET;
    PyObject *py_func_addr = Py_BuildValue(PY_FMT64, func_addr);
    PyObject *py_ret = PyObject_CallFunctionObjArgs(py_meth_analyze, py_func_addr, NULL);
    Py_DECREF(py_func_addr);

    if (py_ret != NULL)
        PyW_PyListListToIntVecVecVec(py_ret, result);

    Py_XDECREF(py_ret);
}

//--------------------------------------------------------------------------
bool PyBBMatcher::FindSimilar(intvec_t &node_list, int_2dvec_t &similar)
{
    PYW_GIL_GET;
    PyObject *py_nodelist = PyW_IntVecToPyList(node_list);
    PyObject *py_ret = PyObject_CallFunctionObjArgs(py_meth_find_similar, py_nodelist, NULL);
    Py_DECREF(py_nodelist);

    if (py_ret == NULL)
        return false;

    bool bOk = PyW_PyListListToIntVecVec(py_ret, similar) == CIP_OK;

    Py_DECREF(py_ret);

    return bOk;
}

//--------------------------------------------------------------------------
bool PyBBMatcher::SaveState(qstring &out)
{
    PYW_GIL_GET;
    PyObject *py_ret = PyObject_CallFunctionObjArgs(py_meth_save_state, NULL);
    if (py_ret == NULL || !PyString_Check(py_ret))
    {
        Py_XDECREF(py_ret);
        return false;
    }

    out = PyString_AsString(py_ret);

    Py_DECREF(py_ret);

    return true;
}

//--------------------------------------------------------------------------
bool PyBBMatcher::LoadState(const char *filename)
{
    PYW_GIL_GET;
    PyObject *py_filename = PyString_FromString(filename);
    PyObject *py_ret = PyObject_CallFunctionObjArgs(py_meth_load_state, py_filename, NULL);
    Py_DECREF(py_filename);
    Py_DECREF(py_ret);

    bool bOk = py_ret == Py_True;

    return bOk;
}