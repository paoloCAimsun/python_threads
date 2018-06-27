#include <Python.h>

#include <thread>
#include <mutex>
#include <chrono>
#include <string>
#include <sstream>
#include <iostream>
#include <regex>

// compilation:
// cythonize program.pyx
// mpic++ pythread.cpp program.c -I/usr/include/python2.7 -lpython2.7 -g -fopenmp  -DDISTRIBUTED

//#define DISTRIBUTED

//#define SHMEM

//#define CYTHON

#ifdef DISTRIBUTED
#include <mpi.h>
#endif

#ifdef CYTHON
#include "program.h"
#endif

// initialize and clean up python
struct initialize
{
    initialize()
    {
        Py_InitializeEx(1);
        PyEval_InitThreads();
    }

    ~initialize()
    {
        Py_Finalize();
    }
};

// acquire GIL
class ensure_gil_state
{
public:
    ensure_gil_state()
    {
        _state = PyGILState_Ensure();
    }

    ~ensure_gil_state()
    {
        PyGILState_Release(_state);
    }

private:
    PyGILState_STATE _state;
};

// allow other threads to run
class enable_threads
{
public:
    enable_threads()
    {
        _state = PyEval_SaveThread();
    }

    ~enable_threads()
    {
        PyEval_RestoreThread(_state);
    }

private:
    PyThreadState* _state;
};

char program[] =
    "import threading\n"
    "def loop():\n"
    "    import time\n"
    "    print \'hi from <thread> \'\n"
    "    time.sleep(5)\n"
    "t = threading.Thread(target=loop)\n"
    "t.start()\n"
;

// run in a new thread
void f()
{
    {
        // grab the GIL
        ensure_gil_state gil_scope;

		// replace the thread ID in the program string
		std::string program_str(program);
		std::regex replace_thread("<thread>");
		auto pid = std::this_thread::get_id();
		std::stringstream ss;
		ss << pid;
		std::string pid_str = ss.str();
		std::string new_program_str=program_str;
		std::regex_replace(new_program_str.begin(), program_str.begin(), program_str.end(), replace_thread, pid_str.c_str());

        // run the Python code above
        PyRun_SimpleString(new_program_str.c_str());
    }

    // let the thread sleep a bit
    //std::this_thread::sleep_for(std::chrono::seconds(1));
}

int main()
{

//C++11 threads
#ifdef SHMEM
	{
		// initialize Python with thread support
		initialize init;

		// start the new thread
		std::thread t1(f);
		// start the new thread
		std::thread t2(f);
		// start the new thread
		std::thread t3(f);

		{
			// release the GIL
			enable_threads enable_threads_scope;

			// wait for the thread to terminate
			t1.join();
			t2.join();
			t3.join();
		}

	}
#endif

//MPI
#ifdef DISTRIBUTED
	Py_Initialize();
	MPI_Init(nullptr, nullptr);
	{
		char program_[] =
			"import threading\n"
			"def loop():\n"
			"    import time\n"
			"    print \'hi from <p> \'\n"
			"    time.sleep(5)\n"
			"t = threading.Thread(target=loop)\n"
			"t.start()\n"
			;

		int pid=0;
		MPI_Comm_rank(MPI_COMM_WORLD, &pid);
		// replace the process ID in the program string
		std::string program_str(program_);
		std::regex replace_thread("<p>");
		std::stringstream ss;
		ss << pid;
		std::string pid_str = ss.str();
		std::string new_program_str=program_str;
		new_program_str.clear();
		std::regex_replace(new_program_str.begin(), program_str.begin(), program_str.end()+1, replace_thread, pid_str.c_str());

        PyRun_SimpleString(new_program_str.c_str());
	}
	MPI_Finalize();
	Py_Finalize();
#endif

#ifdef CYTHON

		// initialize Python with thread support
		initialize init;
		initprogram();

		auto f = [&](){
			{
				ensure_gil_state gil_scope;
				std::mutex lk;
				std::lock_guard<std::mutex> g(lk);
				call();
			}
			// // wait for the Python thread to terminate
			// PyRun_SimpleString("t.join()");
		};

		// start the new thread
		std::thread t1(f);
		// start the new thread
		std::thread t2(f);
		// start the new thread
		std::thread t3(f);

		{
			// release the GIL
			enable_threads enable_threads_scope;

			// wait for the thread to terminate
			t1.join();
			t2.join();
			t3.join();
		}

#endif

    return 0;
}
