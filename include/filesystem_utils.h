/*! @file filesystem_utils.h
 *  @author Santosh Bhattarai
 *  @date 7 July 2017
 */

#ifndef SGNL_FILESYSTEM_UTILS_H
#define SGNL_FILESYSTEM_UTILS_H

#include <dirent.h>
#include <vector>
#include <string>

/* @fn get_files_in_dir
 * @author Santosh Bhattarai
 * @date 7 July 2017
 * @brief Helper function to populate a list of files in directory.
 *		  Inputs: Extension and directory path.
 *		  Output: std::vector<std::string> of filenames
 */
inline void get_files_in_dir(const std::string &dirname, const std::string &ext,
                             std::vector<std::string> &list_of_files)
{
    DIR *dp;
    struct dirent *ep;

    dp = opendir(dirname.c_str());
    if (dp != NULL) {
        while ((ep = readdir(dp))) {
            std::string filename = dirname + ep->d_name;
            size_t found = filename.find(ext);

            // if no match is found the find function returns string::npos
            if (found != std::string::npos) {
                // puts(ep->d_name);
                list_of_files.push_back(filename);
            }
        }
        (void)closedir(dp);
    } else
        perror("Couldn't open the directory");
}

#endif
