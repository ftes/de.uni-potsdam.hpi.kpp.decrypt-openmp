/*
crypt only looks at first 8 characters of plaintext -> drop the rest
use threadsafe crypt_r instead of crypt
*/

#include <stdio.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include <vector>
#include <omp.h>
#include <crypt.h>

using namespace std;

struct Password
{
    string user;
    string salt;
    string password;
};

const unsigned int NUMBER_OF_CHARS_CRYPT = 8;

vector<string> dict;
vector<Password> toCrack;
vector<Password> cracked;

string outFile = "output.txt";

vector<string> parseDict(string fileName)
{
    ifstream file(fileName.c_str());
    vector <string> result;
    string line;
    //only look at first 8 chars of each word
    string lastAcceptedWord = "";
    int dropped = 0;
    while (getline(file, line))
    {
        if (line[line.size() - 1] == '\r')
            line.resize(line.size() - 1);
        string cropped = line.substr(0, NUMBER_OF_CHARS_CRYPT);
        if (cropped != lastAcceptedWord)
        {
            lastAcceptedWord = cropped;
            result.push_back(cropped);
        }
        else dropped++;
    }
    file.close();

    #ifdef DEBUG
    printf("Dropped %d words, because first 8 chars identical\n", dropped);
    #endif

    return result;
}

vector<Password> parsePasswords(string fileName)
{
    ifstream file(fileName.c_str());
    vector <Password> result;
    string line;
    while (getline(file, line))
    {
        Password *p = new Password;
        int i = line.find(":");
        p->user = line.substr(0, i);
        string password = line.substr(i+1, line.length()-1);
        if (password[password.size() - 1] == '\r')
            password.resize(password.size() - 1);
        p->salt = password.substr(0, 2);
        p->password = password;
        result.push_back(*p);
    }
    file.close();
    return result;
}

void writeOutput()
{
    remove(outFile.c_str());
    ofstream output(outFile.c_str());

    for (unsigned int i=0; i<cracked.size(); i++)
    {
        Password p = cracked[i];
        output << p.user << ";" << p.password << "\n";
    }

    output.close();
}

bool cryptAndTestR(string inFile, string salt, string plainText, crypt_data *data)
{
    string expected(crypt_r(plainText.c_str(), salt.c_str(), data));
    return expected == inFile;
}

string testWordCryptR(string word, Password p, crypt_data *data)
{
    if (cryptAndTestR(p.password, p.salt, word, data))
    {
        return word;
    }

    //only build further words, if we are not at max relevant length of word already
    if (word.length() < NUMBER_OF_CHARS_CRYPT)
    {
        for (int j=0; j<10; j++)
        {
            string wordJ = word + to_string(j);
            if (cryptAndTestR(p.password, p.salt, wordJ, data))
            {
                return wordJ;
            }
        }
    }
    return "";
}

void crack()
{
    struct crypt_data data;
    #pragma omp parallel for private(data)
    for (unsigned int i=0; i<toCrack.size(); i++)
    {
        data.initialized = 0;
        string plaintext = "";
        bool found = false;
        for (unsigned int j=0; j<dict.size(); j++)
        {
            if (found) continue;
            plaintext = testWordCryptR(dict[j], toCrack[i], &data);
            if (!plaintext.empty())
            {
                #ifdef DEBUG
                printf("Found password for %s: %s\n", toCrack[i].user.c_str(), plaintext.c_str());
                #endif // DEBUG

                #pragma omp atomic write
                found = true;
                Password newP;
                newP.user = toCrack[i].user;
                newP.password = plaintext;
                #pragma omp critical(cracked)
                cracked.push_back(newP);
            }
        }
    }
}

int main(int argc, char* argv[])
{
    string pwFile = string(argv[1]);
    string dictFile = string(argv[2]);

    dict = parseDict(dictFile);
    toCrack = parsePasswords(pwFile);

    #ifdef DEBUG
    double start = omp_get_wtime();
    #endif

    crack();

    #ifdef DEBUG
    printf("Runtime: %f\n", omp_get_wtime() - start);
    #endif // DEBUG

    writeOutput();

    exit(0);
}
