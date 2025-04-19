#*********************************************************************
#**
#** Name: mvs38_recv.awk
#**
#** Desc: Resolve variables in mvs38_recv.jcl
#**
#*********************************************************************
{
 gsub("XFRUSER", FRUSER)
 gsub("XFRNODE", FRNODE)
 gsub("XTOUSER", TOUSER)
 gsub("XTONODE", TONODE)
 gsub("XFID", FID)
 gsub("XDATE", DATE)
 print
}
