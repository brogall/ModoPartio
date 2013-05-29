#!/usr/bin/env python

import traceback
from os import path



def double_quote(in_string):
    out_string = '"' + in_string + '"'
    return out_string

def main():
    try:
        args = lx.args()

        if args[0] == "SelectFile":
            lx.command( 'dialog.setup',   style = 'fileSave' )
            lx.command( 'dialog.title', title='Select Cache File' )
            lx.command( 'dialog.fileTypeCustom', format='icecache', username='Softimage ICECACHE', loadPattern="*.icecache;", saveExtension="icecache" )
            lx.command( 'dialog.fileTypeCustom', format='bin', username='Realflow BIN', loadPattern="*.bin", saveExtension="bin" )
            lx.command( 'dialog.fileTypeCustom', format='prt', username='Krakatoa PRT', loadPattern="*.prt", saveExtension="prt" )
            lx.command( 'dialog.fileTypeCustom', format='bgeo', username='Houdini BGEO', loadPattern="*.bgeo", saveExtension="bgeo" )     
            lx.command( 'dialog.fileTypeCustom', format='pdc', username='Maya PDC', loadPattern="*.pdc", saveExtension="pdc" )   
            lx.command( 'dialog.fileTypeCustom', format='pda', username='Maya PDA', loadPattern="*.pda", saveExtension="pda" )  
            lx.command( 'dialog.fileTypeCustom', format='pda', username='Maya PDA', loadPattern="*.pda", saveExtension="pda" )                                         
            lx.command( 'dialog.open')    
            cache_path =  lx.eval(  'dialog.result ?' )
            cache_path = path.abspath(cache_path)


            lx.eval('item.channel cacheFileName ' + double_quote(cache_path))


    except:
        lx.out( "Command failed with ", sys.exc_info()[0] )
        lx.out(traceback.format_exc())    

main()