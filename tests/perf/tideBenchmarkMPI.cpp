/*********************************************************************/
/* Copyright (c) 2014, EPFL/Blue Brain Project                       */
/*                     Raphael Dumusc <raphael.dumusc@epfl.ch>       */
/* All rights reserved.                                              */
/*                                                                   */
/* Redistribution and use in source and binary forms, with or        */
/* without modification, are permitted provided that the following   */
/* conditions are met:                                               */
/*                                                                   */
/*   1. Redistributions of source code must retain the above         */
/*      copyright notice, this list of conditions and the following  */
/*      disclaimer.                                                  */
/*                                                                   */
/*   2. Redistributions in binary form must reproduce the above      */
/*      copyright notice, this list of conditions and the following  */
/*      disclaimer in the documentation and/or other materials       */
/*      provided with the distribution.                              */
/*                                                                   */
/*    THIS  SOFTWARE IS PROVIDED  BY THE  UNIVERSITY OF  TEXAS AT    */
/*    AUSTIN  ``AS IS''  AND ANY  EXPRESS OR  IMPLIED WARRANTIES,    */
/*    INCLUDING, BUT  NOT LIMITED  TO, THE IMPLIED  WARRANTIES OF    */
/*    MERCHANTABILITY  AND FITNESS FOR  A PARTICULAR  PURPOSE ARE    */
/*    DISCLAIMED.  IN  NO EVENT SHALL THE UNIVERSITY  OF TEXAS AT    */
/*    AUSTIN OR CONTRIBUTORS BE  LIABLE FOR ANY DIRECT, INDIRECT,    */
/*    INCIDENTAL,  SPECIAL, EXEMPLARY,  OR  CONSEQUENTIAL DAMAGES    */
/*    (INCLUDING, BUT  NOT LIMITED TO,  PROCUREMENT OF SUBSTITUTE    */
/*    GOODS  OR  SERVICES; LOSS  OF  USE,  DATA,  OR PROFITS;  OR    */
/*    BUSINESS INTERRUPTION) HOWEVER CAUSED  AND ON ANY THEORY OF    */
/*    LIABILITY, WHETHER  IN CONTRACT, STRICT  LIABILITY, OR TORT    */
/*    (INCLUDING NEGLIGENCE OR OTHERWISE)  ARISING IN ANY WAY OUT    */
/*    OF  THE  USE OF  THIS  SOFTWARE,  EVEN  IF ADVISED  OF  THE    */
/*    POSSIBILITY OF SUCH DAMAGE.                                    */
/*                                                                   */
/* The views and conclusions contained in the software and           */
/* documentation are those of the authors and should not be          */
/* interpreted as representing official policies, either expressed   */
/* or implied, of Ecole polytechnique federale de Lausanne.          */
/*********************************************************************/

#include "MPIChannel.h"
#include "ReceiveBuffer.h"
#include "serialization/utils.h"

#include <chrono>
#include <iostream>
#include <string>

#include <boost/program_options.hpp>

#define MEGABYTE 1000000
#define RANK0 0

// Example ways to run this program:
// mpirun -n 6 -H localhost ./tideBenchmarkMPI --datasize 60 --packets 100
//
//Object size [Mbytes]: 60
//Time to send 100 objects: 3.445
//Time per object: 0.03445
//Throughput [Mbytes/sec]: 1741.66

namespace
{
class Timer
{
public:
    using clock = std::chrono::high_resolution_clock;

    void start()
    {
        _startTime = clock::now();
    }

    float elapsed() const
    {
        const auto now = clock::now();
        return std::chrono::duration<float>{ now - _startTime }.count();
    }

private:
    clock::time_point _startTime;
};

namespace po = boost::program_options;

struct BenchmarkOptions
{
    BenchmarkOptions( int& argc, char** argv )
        : _desc( "Allowed options" )
        , _getHelp( true )
        , _dataSize( 0 )
        , _packetsCount( 0 )
    {
        initDesc();
        parseCommandLineArguments( argc, argv );
    }

    void showSyntax() const
    {
        std::cout << _desc;
    }

    void initDesc()
    {
        _desc.add_options()
            ("help", "produce help message")
            ("datasize", po::value<float>()->default_value( 0 ),
                     "Size of each data packet [MB]")
            ("packets", po::value<unsigned int>()->default_value( 0 ),
                     "number of packets to transmitt")
        ;
    }

    void parseCommandLineArguments( int& argc, char** argv )
    {
        if( argc <= 1 )
            return;

        po::variables_map vm;
        try
        {
            po::store( po::parse_command_line( argc, argv, _desc ), vm );
            po::notify( vm );
        }
        catch( const std::exception& e )
        {
            std::cerr << e.what() << std::endl;
            return;
        }

        _getHelp = vm.count( "help" );
        _dataSize = vm["datasize"].as<float>() * MEGABYTE;
        _packetsCount = vm["packets"].as<unsigned int>();
    }

    po::options_description _desc;

    bool _getHelp;
    unsigned int _dataSize;
    unsigned int _packetsCount;
};
}

/**
 * Send data via MPI to benchmark the effective link speed at application level.
 */
int main( int argc, char** argv )
{
    BenchmarkOptions options( argc, argv );
    if( options._getHelp )
    {
        options.showSyntax();
        return 0;
    }

    MPIChannel mpiChannel( argc, argv );

    // Send buffer
    std::vector<char> noiseBuffer( options._dataSize );
    for( auto& elem : noiseBuffer )
        elem = rand();
    const auto serializedData = serialization::toBinary( noiseBuffer );

    // Receive buffer
    ReceiveBuffer buffer;
    Timer timer;
    size_t counter = 0;

    mpiChannel.globalBarrier();
    timer.start();

    while( counter < options._packetsCount )
    {
        if( mpiChannel.getRank() == RANK0 )
            mpiChannel.broadcast( MPI_MESSAGE_TYPE_NONE, serializedData );
        else
        {
            const MPIHeader header = mpiChannel.receiveHeader( RANK0 );
            buffer.setSize( header.size );
            mpiChannel.receiveBroadcast( buffer.data(), header.size, RANK0 );
        }
        ++counter;
    }

    const float time = timer.elapsed();

    if( mpiChannel.getRank() == RANK0 )
    {
        std::cout << "Object size [Mbytes]: " << (float)serializedData.size() / MEGABYTE << std::endl;
        std::cout << "Time to send " << counter << " objects: " << time << std::endl;
        std::cout << "Time per object: " << time / counter << std::endl;
        std::cout << "Throughput [Mbytes/sec]: " << counter * serializedData.size() / time / MEGABYTE << std::endl;
    }

    return 0;
}
