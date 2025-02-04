#ifndef AVP_IMG_RECEIVE_MPS_MESSAGE_H
#define AVP_IMG_RECEIVE_MPS_MESSAGE_H
#include <memory>

namespace mps
{

    /**
     * General message class. Derive your message from this class
     */
    class message {
    public:
        virtual ~message() = default;
    };

    using message_sptr = std::shared_ptr<message>;
    using message_csptr = std::shared_ptr<const message>;
}


#endif //AVP_IMG_RECEIVE_MPS_MESSAGE_H
