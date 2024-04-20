#ifndef JOB_HPP
#define JOB_HPP

#include "../App.hpp"

class Job	: public AppAccess {
public:
	Job();

	Job& create_listener();

	Job& post_command();


};

#endif // JOB_HPP
