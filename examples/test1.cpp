#include <stdio.h>
#include "../dbf.h"

int main( )
{
    CDBFTable table;

    if ( table.Open("user.dbf") )
    {
        table.GoTop();
        while ( !table.EndOfFile() )
        {
            if ( !table.Read() ) // read record data
            {
                // error
            }
            
            printf("%d\n", table.GetInteger("id"));        // get integer
            printf("%s\n", table.GetChar("name"));         // get char
            printf("%f\n", table.GetNumeric(2));           // get numeric (by field index)
            
            unsigned int size;
            const void *m = table.GetMemo("note", &size);  // get MEMO data
            if ( m )
            {
                // m    - MEMO data
                // size - MEMO size
                // Don't free this memory pointer! It will be reallocated during next call and released when table closed.
                // Just copy data into your buffer.
            }
            
            table.Skip(); // next record
        }
        table.Close();    // close table
    }
    
    return 0;
}
