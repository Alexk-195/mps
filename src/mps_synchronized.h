#ifndef AVP_IMG_RECEIVE_MPS_SYNCHRONIZED_H
#define AVP_IMG_RECEIVE_MPS_SYNCHRONIZED_H

#include <mutex>

namespace mps
{
    /// Wrapper which adds locking to access complex data from threads.
    /// Adds synchronized methods for copying data to and from the class.
    template<class T>
    class synchronized {
    private:
        /// protects data
        mutable std::mutex mutex;
        /// copy of data
        T data;
    public:

        /// copies src to internal copy
        void synced_copy_from(const T &src) {
            std::lock_guard<std::mutex> lock(mutex);
            data = src;
        }

        /// copies internal copy to dest
        void synced_copy_to(T *dest) const {
            std::lock_guard<std::mutex> lock(mutex);
            *dest = data;
        }
    };
}

#endif //AVP_IMG_RECEIVE_MPS_SYNCHRONIZED_H
