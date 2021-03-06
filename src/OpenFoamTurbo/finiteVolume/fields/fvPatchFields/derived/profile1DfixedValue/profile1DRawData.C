/*---------------------------------------------------------------------------*\
 =========                   |
 \\      /   F ield          | OpenFOAM: The Open Source CFD Toolbox
  \\    /    O peration      |
   \\  /     A nd            | Copyright Hydro-Quebec - IREQ, 2008
    \\/      M anipulation   |
-------------------------------------------------------------------------------
License
    This file is part of OpenFOAM.

    OpenFOAM is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the
    Free Software Foundation; either version 2 of the License, or (at your
    option) any later version.

    OpenFOAM is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License
    along with OpenFOAM; if not, write to the Free Software Foundation,
    Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

Class
    profile1DRawData

Description
    Read an ASCII file containing a 1D radial/vertical profile

    File format supported:
            turboCSV: the same kind as the ASCII CSV file format generated by CFX-5
	    Other file format could be added eventually

    turboCSV file header:
    ---------------------
    The header of the file must contain the string [Data] on a single line, then a list of comma-separated
    field names with an optional unit description. The optional unit description will be discarded.

    Here is a few examples of valid turboCSV file headers:


[Data]
 R [ m ], Velocity Axial [ m s^-1 ], Velocity Radial [ m s^-1 ], Velocity Circumferential [ m s^-1 ], Pressure [ Pa ], Turbulence Kinetic Energy [ m^2 s^-2 ], Turbulence Eddy Dissipation [ m^2 s^-3 ]

or
 
[Data]
 Z [ m ], Velocity Axial [ m s^-1 ], Velocity Radial [ m s^-1 ], Velocity Circumferential [ m s^-1 ], Pressure [ Pa ], Turbulence Kinetic Energy [ m^2 s^-2 ], Turbulence Eddy Dissipation [ m^2 s^-3 ]

or

[Data]
 R, Velocity Axial, Velocity Radial, Velocity Circumferential, Pressure, Turbulence Kinetic Energy, Turbulence Eddy Dissipation

or

[Data]
 R, Pressure

or

[Data]
 Z, Velocity Axial, Velocity Radial, Velocity Circumferential


    turboCSV file field values:
    ---------------------------
    Following this file header, a list of comma-separated field values must be provided,
    one entry per line or one entry per R or Z values.
    On each line, there must be as many entries as there are field names provided.

    For a given line of field values, the nth entry on the line corresponds to a value for
    the nth field name from the file header.

    There must be at least 2 lines of field values, in increasing values of R or Z.

    The min and max values for R or Z define the range over which the interpolateCoord values
    will be interpolated.

 
    
    turboCSV file format: a complete example
    ---------------------------------------
    See the file profileExample.csv for a complete example of the turboCSV file format.



Authors 
    Martin Beaudoin, Hydro-Quebec - IREQ
    Maryse Page,     Hydro-Quebec - IREQ
    Robert Magnan,   Hydro-Quebec - IREQ

\*---------------------------------------------------------------------------*/

#include <unistd.h>
#include <iostream>
#include <fstream>                        

#include "error.H"
#include "profile1DRawData.H"

namespace Foam
{
//- Some utility functions
// Function for the conversion to enum for each field type for the turbo CSV file
template<class TypeEnum, class TypeContainerXlate>
const TypeEnum string_to_turboCSVEnum(std::string s_field, TypeContainerXlate mapXlateValues)
{
    // Default value
    TypeEnum retValue = TypeEnum(0);

    typename TypeContainerXlate::iterator p_m_string_to_turboCSVEnum;

    p_m_string_to_turboCSVEnum = mapXlateValues.find( toLower(s_field));

    if(p_m_string_to_turboCSVEnum != mapXlateValues.end())
        retValue = p_m_string_to_turboCSVEnum->second;

    return retValue;
}

// Function for the conversion to enum for each profile type
profile1DType string_to_profile1DType(std::string s_type)
{
    // Construct the map
    static m_profile1DTypeXlate_from_string m_string_to_profile1DType(	
        profile1DTypeXlate_map_init,
        profile1DTypeXlate_map_init +
        sizeof(profile1DTypeXlate_map_init) /
        sizeof(profile1DTypeXlate_map_init[0]) );

    return string_to_turboCSVEnum<profile1DType, m_profile1DTypeXlate_from_string>(s_type, m_string_to_profile1DType);
}

// Function for the conversion to enum for each field type
profile1DField string_to_profile1DField(std::string s_field)
{
    // Construct the map
    static m_profile1DFieldXlate_from_string m_string_to_profile1DField(
        profile1DFieldXlate_map_init,
        profile1DFieldXlate_map_init +
        sizeof(profile1DFieldXlate_map_init) /
        sizeof(profile1DFieldXlate_map_init[0]));

    return string_to_turboCSVEnum<profile1DField, m_profile1DFieldXlate_from_string>(s_field, m_string_to_profile1DField);
}


// Initialization of the non integral constants
const std::string profile1DRawData::KEY_R                        = "R";
const std::string profile1DRawData::KEY_Z                        = "Z";
const std::string profile1DRawData::KEY_VELOCITY_AXIAL           = "Velocity Axial";
const std::string profile1DRawData::KEY_VELOCITY_RADIAL          = "Velocity Radial";
const std::string profile1DRawData::KEY_VELOCITY_CIRCUMFERENTIAL = "Velocity Circumferential";
const std::string profile1DRawData::KEY_PRESSURE                 = "Pressure";
const std::string profile1DRawData::KEY_TKE                      = "Turbulence Kinetic Energy";
const std::string profile1DRawData::KEY_EPSILON                  = "Turbulence Eddy Dissipation";
const std::string profile1DRawData::KEY_OMEGA                    = "Turbulence Specific Dissipation Rate";

// Constructor
profile1DRawData::
profile1DRawData(std::string nameFile, std::string typeFile) :
    nameFile_(nameFile),
    typeFile_(typeFile),
    is_validFlag(false)
{
    bool verbose = false;

    // Validation of the file path 
    if( access(nameFile_.c_str(), R_OK)==0 )
    {
        // File reading
        std::ifstream file(nameFile_.c_str());

        if(typeFile.compare("turboCSV") == 0)
        {
            std::string buffer;

            if(verbose)
                Info << "    profile1DRawData:: Reading file : " << nameFile_ << " in turboCSV format" << endl;

            while(file.good())
            {
                std::getline(file, buffer);

                if(*(buffer.c_str()) == '#')
                {
                    header_.push_back(buffer);
                }
                else if(buffer.find("[Name]") == 0)
                {
                    std::getline(file, name_);
                }
                else if(buffer.find("[Spatial Fields]") == 0)
                {
                    std::getline(file, spatial_fields_);
                }
                else if(buffer.find("[Data]") == 0)
                {
                    // Read the profile data
                    std::getline(file, buffer);

                    // Extract the tokens
                    std::list<std::string> tokens_;
                    extractListTokens_TURBO_CSV(buffer, tokens_);

                    // The list and the name of the tokens are known. Lets read the data
                    std::list<double>      f_values_[tokens_.size()];

                    while(file.good())  // Read until the end of the file
                    {
                        std::getline(file, buffer);

                        // Extract the turboCSV list
                        std::list<std::string> s_values_;
                        extractListTokens_TURBO_CSV(buffer, s_values_);

                        // We need as many s_values_ as we have tokens_
                        // Otherwise, we have a blank line , or missing data values in some columns
                        if(s_values_.size() == tokens_.size())
                        {
                            int index_ = 0;
                            for(std::list<std::string>::iterator p_s_values_=s_values_.begin();
                                p_s_values_!=s_values_.end();
                                ++p_s_values_)
                            {
                                char* dummy;
                                double d = strtod( (*p_s_values_).c_str(), &dummy );
                                f_values_[index_++].push_back(d);
                            }
                        }
                        else if(buffer.size() > 0)  // Don't complain if only empty lines with \n
                        {
                            // Just generate a warning so people will know the CSV file got
                            // some junk in the Data section
                            Info << "    profile1DRawData(): "
                                << " CVS file : " << nameFile_
                                << " Skipping this line in Data section: " << buffer << endl;
                        }
                    }

                    // Transfert in profile_
						
                    int i=0;
                    std::list<std::string>::iterator p_tokens_ = tokens_.begin();

                    while(p_tokens_ != tokens_.end())
                    {
                        ProfileValues profile_values;

                        profile_values.values.resize(f_values_[i].size());

                        std::list<double>::iterator p_values_ = f_values_[i].begin();

                        int j = 0;
                        while(p_values_ != f_values_[i].end())
                        {
                            profile_values.values[j++] = *p_values_;
                            p_values_++;
                        }

                        std::string nameParam;
                        extractListNameParam_Units(*p_tokens_, nameParam, profile_values.units);

                        // Create an association name_parameters, values
                        profile_.insert(m_t_Profile_::value_type(nameParam, profile_values));

                        p_tokens_++;
                        i++;
                    }
                }
                else
                {
                    // Nothing
                }
            }

            is_validFlag = true;
        }
        else  // other format
        {
            FatalErrorIn("profile1DRawData::profile1DRawData:  ")
                << "This format is not implemented yet : " << typeFile
                    << exit(FatalError);
        }

        // File closing
        file.close();
    }
    else
    {
        FatalErrorIn("profile1DRawData::profile1DRawData:  ")
            << "Non existing file: " << nameFile
                << exit(FatalError);
 
    }
			
}

inline bool is_white( char c ) { return (c==' '|| c=='\t'); }
inline void strip_whitespaces( std::string& s )
{
    // Search the first non-whitespace
    std::string::const_iterator beg=s.begin();
    while( beg!= s.end() )
    {
        if ( !is_white( *beg ) ) break;
        beg++;
    }
    // From there, search for the last non-whitespace
    std::string::const_iterator end=beg;
    std::string::const_iterator p=beg;
    while( p != s.end() )
    {
        if ( !is_white( *p ) ) end=p;
        p++;
    }
    s.assign( beg, ++end );
}
	
// Function to extract a list of tokens separated by comma (CSV format)
void profile1DRawData::extractListTokens_TURBO_CSV(
    std::string s_buffer,
    std::list<std::string> &l_tokens_
)
{
    char* buf = strdup( s_buffer.c_str() ); // makes a modifiable copy of the string
    char* keep = buf; // keep a pointer to this free-store
    char* s;
    while( (s=strtok( buf, "," )) != NULL )
    {
        std::string token_( s );
        strip_whitespaces( token_ );
        l_tokens_.push_back(token_);
			
        buf = NULL;
    }
    if ( keep ) free( keep );
    return;
}

// Function to separate the parameter name from the units:
// Example : Velocity Axial [ m s^-1 ]  gives:
// first  : Velocity Axial
// second : m s^-1
//
void profile1DRawData::extractListNameParam_Units(
    std::string  s_buffer,
    std::string& nameParam,
    std::string& nameUnits
)
{
    char* buf = strdup( s_buffer.c_str() ); // makes a modifiable copy of the string
    char* keep = buf; // keep a pointer to this free-store
    char* s;
    bool first = true;
    while( (s=strtok( buf, "[]" )) != NULL )
    {
        std::string token_( s );
        strip_whitespaces( token_ );
        if ( first )
            nameParam = token_;
        else
            nameUnits = token_;
        first = false;
        buf = NULL;
    }
    if ( keep ) free( keep );
    return;
}

// Functions that use predetermined keys for return the data for the turbo CSV format
// By default, we exit with a FatalError if the key is invalid, but this can be overriden if needed
int profile1DRawData::get_values(
    std::string key,
    std::vector<double> &values,
    bool exitOnInvalidKey)
{
    int retCode = 0;
    m_t_Profile_::iterator p = profile_.find(key);

    if(p != profile_.end())
    {
        values = (*p).second.values;
    }
    else if (exitOnInvalidKey)   // Invalid key
    {
        // We gracefully abort
        FatalErrorIn                                     \
            (                                                \
                "profile1DRawData::get_values::get_values()"           \
            ) << ": The file : " << nameFile_        \
                << " does not contain values for : "   \
                << key << abort(FatalError);    \
    }		    
    else
        retCode = TURBO_CSV_INVALID_VALUE_KEY;

    return (retCode);
}

int profile1DRawData::get_z(std::vector<double>                       &z)
{
    return get_values(KEY_Z, z); // With validation by default 
}

int profile1DRawData::get_r(std::vector<double>                       &r)
{
    return get_values(KEY_R, r); // With validation by default 
}

int profile1DRawData::get_velocityAxial(std::vector<double>           &v_axial)
{
    return get_values(KEY_VELOCITY_AXIAL, v_axial); // With validation by default 
}

int profile1DRawData::get_velocityRadial(std::vector<double>          &v_radial)
{
    return get_values(KEY_VELOCITY_RADIAL, v_radial); // With validation by default 
}

int profile1DRawData::get_velocityCircumferential(std::vector<double> &v_circum)
{
    return get_values(KEY_VELOCITY_CIRCUMFERENTIAL, v_circum); // With validation by default 
}

int profile1DRawData::get_pressure(std::vector<double>                &pressure)
{
    return get_values(KEY_PRESSURE, pressure); // With validation by default 
}

int profile1DRawData::get_tke(std::vector<double>                     &tke)
{
    return get_values(KEY_TKE, tke); // With validation by default 
}

int profile1DRawData::get_epsilon(std::vector<double>                 &epsilon)
{
    return get_values(KEY_EPSILON, epsilon); // With validation by default 
}
	
int profile1DRawData::get_omega(std::vector<double>                   &omega)
{
    return get_values(KEY_OMEGA, omega); // With validation by default 
}
	
} // End namespace Foam
