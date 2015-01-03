#include <Python.h>
#include <git2.h>
#include <git2/odb_backend.h>

/* Declare other symbols that are going to be linked into this module. In an
 * ideal world the backends repo would export these via a header, that can be
 * worked towards */

int git_odb_backend_mysql_open(git_odb_backend **backend_out,
         const char *mysql_host,
         const char *mysql_user, const char *mysql_passwd, const char *mysql_db,
         unsigned int mysql_port, const char *mysql_unix_socket,
	 unsigned long mysql_client_flag);

int git_odb_backend_mysql_create(const char *mysql_host,
         const char *mysql_user, const char *mysql_passwd, const char *mysql_db,
         unsigned int mysql_port, const char *mysql_unix_socket,
	 unsigned long mysql_client_flag);

void git_odb_backend_mysql_free(git_odb_backend *backend);

PyObject *
open_mysql_backend(PyObject *self, PyObject *args)
{
  const char *host, *user, *passwd, *sql_db, *unix_socket;
  git_odb_backend *backend = NULL;
  git_odb *odb = NULL;
  git_repository *repository = NULL;
  int portno, ret = GIT_OK;

  if (!PyArg_ParseTuple(args, "ssssiz", &host, &user, &passwd, &sql_db,
			  &portno, &unix_socket))
    return NULL;

  /* XXX -- allow for connection options such as compression and SSL */
  ret = git_odb_backend_mysql_open(&backend, host, user, passwd, sql_db, portno,
		  unix_socket, 0);
  if (ret == GIT_ENOTFOUND) {
    PyErr_Format(PyExc_Exception, "No git db found in specified database");
    return NULL;
  } else if (ret < 0) {
    /* An error occurred -- XXX however there's currently no facility for
     * identifying what error that is and telling the user about it, which is
     * poor. For now, just raise a generic error */
    PyErr_Format(PyExc_Exception, "Failed to connect to mysql server");
    return NULL;
  }

  /* We have successfully created a custom backend. Now, create an odb around
   * it, and then wrap it in a repository. */
  ret = git_odb_new(&odb);
  if (ret != GIT_OK)
    goto cleanup;

  ret = git_odb_add_backend(odb, backend, 0);
  if (ret != GIT_OK)
    goto cleanup;

  ret = git_repository_wrap_odb(&repository, odb);
  if (ret != GIT_OK)
    goto cleanup;

  /* On success, return a PyCapsule containing the created repo.
   * No destructor, manual deallocation occurs */
  return PyCapsule_New(repository, "", NULL);

cleanup:
  if (odb)
    git_odb_free(odb); /* This frees the backend too */
  else if (backend)
    git_odb_backend_mysql_free(backend);

  PyErr_Format(PyExc_Exception,
		  "Git error %d during construction of git repo", ret);
  return NULL;
}

PyObject *
create_mysql_backend(PyObject *self, PyObject *args)
{
  const char *host, *user, *passwd, *sql_db, *unix_socket;
  int portno, ret;

  if (!PyArg_ParseTuple(args, "ssssiz", &host, &user, &passwd, &sql_db,
			  &portno, &unix_socket))
    return NULL;

  /* XXX -- allow for connection options such as compression and SSL */
  ret = git_odb_backend_mysql_create(host, user, passwd, sql_db, portno,
                                     unix_socket, 0);
  if (ret < 0) {
    /* An error occurred -- XXX however there's currently no facility for
     * identifying what error that is and telling the user about it, which is
     * poor. For now, just raise a generic error */
    PyErr_Format(PyExc_Exception, "Failed to create git db");
    return NULL;
  }

  Py_RETURN_NONE;
}

PyMethodDef module_methods[] = {
  {"open_mysql_backend", open_mysql_backend, METH_VARARGS, NULL},
  {"create_mysql_backend", create_mysql_backend, METH_VARARGS, NULL},
  {NULL}
};


#if PY_MAJOR_VERSION < 3
PyMODINIT_FUNC
init_pygit2_backends(void)
{
  PyObject* m;
  m = Py_InitModule3("_pygit2_backends", module_methods,
                     "Backend facilities for pygit2");
  (void)m;
  return;
}
#else
struct PyModuleDef moduledef = {
  PyModuleDef_HEAD_INIT,
  "_pygit2_backends",              /* m_name */
  "Backend facilities for pygit2", /* m_doc */
  -1,                              /* m_size */
  module_methods,                  /* m_methods */
  NULL,                            /* m_reload */
  NULL,                            /* m_traverse */
  NULL,                            /* m_clear */
  NULL,                            /* m_free */
};

PyMODINIT_FUNC
init_pygit2_backends(void)
{
  PyObject* m;
  m = PyModule_Create(&moduledef);
  return m;
}
#endif
