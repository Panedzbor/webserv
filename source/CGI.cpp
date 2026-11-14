#include "CGI.hpp"
#include <iostream>
#include <ctime>

CGI::CGI(const HttpRequest & request, HttpResponse & response, const LocationConfig & location, const std::string & cgi_extension, std::map<int, CGI*>& cgiMap)
:   extension(cgi_extension),
    interpreterPath(location.cgi_handlers.find(cgi_extension)->second),
    response(response),
    clientsCGImap(cgiMap),
    cgi_read_fd(-1),
    cgi_write_fd(-1),
    pid(-2),
    responseReady(false)
{
    std::cout << "Constructing CGI object\n";
    setArgv(request, location);
    setEnvp(request);
}

// resolves path according to configs
void CGI::resolveScriptPath(const HttpRequest & request, const LocationConfig & location)
{
    scriptPath += location.root;
    if (scriptPath.empty() || scriptPath[scriptPath.size()-1] != '/')
        scriptPath += '/'; // ensures slash at the beginning
    std::string tempPath = request.getPath();
    if (tempPath.size() && tempPath[0] == '/')
        scriptPath.append(tempPath.substr(1));
    else // eliminates double-slash
        scriptPath += tempPath;
}

void CGI::setArgv(const HttpRequest & request, const LocationConfig & location)
{
    resolveScriptPath(request, location);
    std::cout << "Script path: " << scriptPath << "\n";

    argv.push_back(const_cast<char*>(interpreterPath.c_str()));
    argv.push_back(const_cast<char*>(interpreterPath.c_str()));
    argv.push_back(const_cast<char*>(scriptPath.c_str()));
    argv.push_back(NULL);
}

// prepares environmental arguments for execve (implemented as per current needs)
void CGI::setEnvp(const HttpRequest & request/*, std::vector<std::string> envp_required*/) /*in case other cgi scripts will be supported*/
{
    std::string tempEnvp[3];

    // size_t s = envp_required.size();
    //for (size_t i = 0; i < s; i++) {
    tempEnvp[0] = "\"REQUEST_METHOD=" + request.getMethod() + "\"";
    tempEnvp[1] = "\"PATH=" + request.getPath() + "\"";
    if (request.getHeaders().find("content-length") != request.getHeaders().end())
        tempEnvp[2] = "\"CONTENT_LENGTH=" + request.getHeaders().find("content-length")->second + "\"";
    else
        tempEnvp[2] = "";
    
    envp.push_back(const_cast<char*>(tempEnvp[0].c_str()));
    envp.push_back(const_cast<char*>(tempEnvp[1].c_str()));
    if (!tempEnvp[2].empty())
        envp.push_back(const_cast<char*>(tempEnvp[2].c_str()));
    envp.push_back(NULL);
    //}
    std::cout << "envp: " << envp[0] << " " << envp[1]; if (envp[2]) std::cout << " " << envp[2]; std::cout << "\n";
}

void CGI::runScript()
{
    // create pipes, fork, etc, and run execve
    int pipeWriteToCGI[2] = {-1, -1};
    int pipeReadFromCGI[2] = {-1, -1};
    if (pipe(pipeWriteToCGI) == -1 || pipe(pipeReadFromCGI) == -1)
    {
        //handleError();
        return;
    }

    pid_t pid = fork();
    if (pid == -1)
    {
        //handleError();
        return;
    }
    else if (pid == 0)
    {
        if (dup2(pipeWriteToCGI[0], STDIN_FILENO) == -1 || dup2(pipeReadFromCGI[1], STDOUT_FILENO) == -1)
        {//handle error
        }
        close(pipeWriteToCGI[0]);
        close(pipeReadFromCGI[1]);
        close(pipeWriteToCGI[1]);
        close(pipeReadFromCGI[0]);
        execve(scriptPath.c_str(), &argv[0], &envp[0]);
        //handleError(); + exit + check regularly
        std::string buf = "CGI_ERROR"; // define
        write(STDOUT_FILENO, buf.c_str(), buf.size());
        char c;
        read(STDIN_FILENO, &c, 1); //dont close pipe before kill
    }
    close(pipeWriteToCGI[0]);
    close(pipeReadFromCGI[1]);
    // save cgi_fd (= parent listening fd)
    cgi_read_fd = pipeReadFromCGI[0];
    cgi_write_fd = pipeWriteToCGI[1];
    this->pid = pid; 
    // record parent listening fd in cgiMap (to add it to poll)
    clientsCGImap[cgi_read_fd] = this;
}

// reads until EOF then builds response
void CGI::processOutput()
{
    // readOutput();
    // after EOF:
    // buildResponse();
        // responseReady = true;
    // cleanup cgi object: close fd, remove from cgi map
}

bool CGI::isResponseReady() const
{
    return responseReady;
}

void CGI::handleError()
{

}