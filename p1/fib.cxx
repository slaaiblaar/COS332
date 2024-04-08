#include <iostream>
#include <fstream>
#include <sstream>
#include <string>

int main(int argc, char *argv[]) {
	std::cout << "Content-type: text/html\n\n";
	std::ifstream fibFileInput("fibnums.txt");
	if (!fibFileInput.is_open()) {
		std::cout                                                                                
	                << "<!DOCTYPE html>"
	                << "<html>"                                                     
	                <<      "<head><title>COS332 P1</title></head>"                
	                <<      "<body>"
	                <<              "Error: Could not open file \"fibnums.txt\""
	                <<      "</body>"
	                << "</html>";
		return 1;
	}
	
	std::string argument(argv[argc - 1]);

	std::string line ("empty");
	std::getline(fibFileInput, line);

	std::stringstream stream(line);
	int f0, f1, f2;

	if (!(stream >> f0 >> f1 >> f2)) {
		std::cout   
                        << "<!DOCTYPE html>"
                        << "<html>"                                             
                        <<      "<head><title>COS332 P1</title></head>"        
                        <<      "<body>"
                        <<              "Error: Could not read numbers from file \"fibnums.txt\""          
                        <<      "</body>"
                        << "</html>";
                return 1;
	}
	fibFileInput.close();
	//std::cout << argument << std::endl;
	bool notInitialVals = f0 != 0 || f1 != 1 || f2 != 1;
	//std::cout << f0 << ", " << f1 << ", " << f2 << std::endl;
	if (argc == 1 || (f0 == 0 && f1 == 0 && f2 == 0)) {
		f0 = 0;
		f1 = f2 = 1;
	} else if (argument.compare("next") == 0) {
		f2 = (f0 = f1) + (f1 = f2);
	} else if (argument.compare("prev") == 0 && notInitialVals) {
		f0 = (f2 = f1) - (f1 = f0);
	}

	else 
	{
		f0 = 0;
		f1 = f2 = 1;
	}

	notInitialVals = f0 != 0 || f1 != 1 || f2 != 1;

	std::ofstream fibFileOutput("fibnums.txt");
	if (!fibFileOutput.is_open()) {
                std::cout                                                                                
                        << "<!DOCTYPE html>"
                        << "<html>"                                                     
                        <<      "<head><title>COS332 P1</title></head>"                
                        <<      "<body>"
                        <<              "Error: Could not open file for writing\"fibnums.txt\""
                        <<      "</body>"
                        << "</html>";
                return 1;
        }
	
	fibFileOutput << f0 << " " << f1 << " " << f2 << std::endl;
	fibFileOutput.close();
	
	std::cout                                                                                
                << "<!DOCTYPE html>"
                << "<html>"                                                     
                <<      "<head><title>COS332 P1</title></head>"                
                <<      "<body>"
		<<		"<a href=\"/cgi-bin/fib?prev\"" << (!notInitialVals ? " style=\"pointer-events: none\"" : "") << ">Previous</a>"
                <<              "(" << f0 << ", " << f1 << ", " << f2 << ")"
		<<		"<a href=\"/cgi-bin/fib?next\">Next</a>"
                <<      "</body>"
                << "</html>";

        return 0;
} 
