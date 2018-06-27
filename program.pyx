import sys
import time
import threading

cdef public void call():
    def loop():
        import time
        print "hi from <thread> "
        time.sleep(5)
        print "end "
    t = threading.Thread(target=loop)
    t.start()
    t.join()
