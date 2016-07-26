//------------------------------------------------------------------------------
// File: Event server impl.hpp
//
// Desc: Event server
//
// Copyright (c) 2014-2018 opencvr.com. All rights reserved.
//------------------------------------------------------------------------------

#ifndef __VSC_EVENT_SERVER_IMPL_H_
#define __VSC_EVENT_SERVER_IMPL_H_

inline VEventServerCallbackTask::VEventServerCallbackTask(Factory &pFactory)
:m_Factory(pFactory)
{
}

inline VEventServerCallbackTask::~VEventServerCallbackTask()
{

}

inline void VEventServerCallbackTask::PushEvent(VEventData &pData)
{
	m_Queue.Push(pData);

	return;
}
/* Register the call back for the event */
inline BOOL VEventServerCallbackTask::RegEventNotify(void * pData, FunctionEventNotify callback)
{
	XGuard guard(m_cMutex);
	m_NotifyMap[pData] = callback;

	return TRUE;
}
inline BOOL VEventServerCallbackTask::UnRegEventNotify(void * pData)
{
	XGuard guard(m_cMutex);
	m_NotifyMap.erase(pData);

	return TRUE;
}

inline void VEventServerCallbackTask::run()
{
	while(m_Queue.BlockingPeek() == true)
	{
		VEventData pData = m_Queue.Pop();
		VDC_DEBUG( "%s Pop a Event \n",__FUNCTION__);
		/* Call the callback */
		XGuard guard(m_cMutex);
		FunctionEventNotifyMap::iterator it = m_NotifyMap.begin(); 
		for(; it!=m_NotifyMap.end(); ++it)
		{
			if ((*it).second)
			{
				(*it).second(pData, (*it).first);
			}
		}
	}
}

inline VEventServerSearchTask::VEventServerSearchTask(Factory &pFactory, VEventServerDbTask &pDbTask)
:m_Factory(pFactory), m_pDbTask(pDbTask)
{
}

inline VEventServerSearchTask::~VEventServerSearchTask()
{

}

inline BOOL VEventServerSearchTask::PushCmd(VVidEventSearchCmd &pCmd)
{
	m_Queue.Push(pCmd);

	return TRUE;
}

/* Register the call back for the event */
inline BOOL VEventServerSearchTask::RegEventNotify(void * pData, FunctionEventNotify callback)
{
	XGuard guard(m_cMutex);
	m_NotifyMap[pData] = callback;

	return TRUE;
}
inline BOOL VEventServerSearchTask::UnRegEventNotify(void * pData)
{
	XGuard guard(m_cMutex);
	m_NotifyMap.erase(pData);

	return TRUE;
}

inline BOOL VEventServerSearchTask::ProcessSearchCmd(VVidEventSearchCmd &pCmd)
{

	Poco::Data::Session *m_DB = m_pDbTask.GetDataSession();
	/* Can not search the event db */
	if (m_DB == NULL)
	{
		m_pDbTask.ReleaseDataSession();
		return FALSE;
	}

	s64 recordId = 0;
	s64 recordLastId = 0;
	while (1)
	{
	    std::vector<s64> recordIds;
	    
	    Statement stmt = (*m_DB << "SELECT id FROM events WHERE strDevice=:strDevice AND id >:recordid AND \
	(nTime>=:starttime AND  nTime<=:endtime)", 
	use(pCmd.strId), use(recordLastId), use(pCmd.nStart), use(pCmd.nEnd), into(recordIds), limit(1));
	    stmt.execute();
		VDC_DEBUG( "%s  %d recordLastId %d \n",__FUNCTION__, __LINE__, 
					recordLastId);
		
	    if (recordIds.size() > 0)
	    {
			VDC_DEBUG( "%s  %d recordLastId %d \n",__FUNCTION__, __LINE__, 
					recordLastId);	   
			VEventData sEvent;
			std::vector<s64>::iterator it = recordIds.begin();
			recordId = *it;
			sEvent.strDevice = pCmd.strId;
#if 1
		*m_DB << "SELECT strId FROM events WHERE id=:id", use(recordId), 
		   into(sEvent.strId), now;
		*m_DB << "SELECT strDeviceName FROM events WHERE id=:id", use(recordId), 
		   into(sEvent.strDeviceName), now;
		*m_DB << "SELECT strType FROM events WHERE id=:id", use(recordId), 
		   into(sEvent.strType), now;
		*m_DB << "SELECT nTime FROM events WHERE id=:id", use(recordId), 
		   into(sEvent.nTime), now;
		*m_DB << "SELECT strEvttime FROM events WHERE id=:id", use(recordId), 
		   into(sEvent.strEvttime), now;
		*m_DB << "SELECT strDesc FROM events WHERE id=:id", use(recordId), 
		   into(sEvent.strDesc), now;
		*m_DB << "SELECT bHandled FROM events WHERE id=:id", use(recordId), 
		   into(sEvent.bHandled), now;
		*m_DB << "SELECT strComments FROM events WHERE id=:id", use(recordId), 
		   into(sEvent.strComments), now;
#else
		*m_DB << "SELECT strId strDeviceName strType nTime strEvttime strDesc \
			bHandled strComments FROM events WHERE id=:id", use(recordId), 
		   into(sEvent.strId), into(sEvent.strDeviceName), into(sEvent.strType), into(sEvent.nTime),
		   into(sEvent.strEvttime), into(sEvent.strDesc), into(sEvent.bHandled),
		   into(sEvent.strComments), now;
#endif
			XGuard guard(m_cMutex);
			FunctionEventNotifyMap::iterator it2 = m_NotifyMap.begin(); 
			for (; it2 != m_NotifyMap.end(); ++it2)
			{
				if ((*it2).second)
				{
					if (pCmd.pData == (*it2).first)
					{
						/* The data is search from database */
						(*it2).second(sEvent, (*it2).first);
						break;
					}
				}
			}
			recordLastId = recordId;
	    }else
	    {
		break;
	    }

	}
	m_pDbTask.ReleaseDataSession();
	return TRUE;
}


inline BOOL VEventServerSearchTask::ProcessHandleCmd(VVidEventSearchCmd &pCmd)
{
	Poco::Data::Session *m_DB = m_pDbTask.GetDataSession();
	/* Can not search the event db */
	if (m_DB == NULL)
	{
		m_pDbTask.ReleaseDataSession();
		return FALSE;
	}
	bool bHandled = true;
	/* Set handle true */
	*m_DB << "UPDATE events SET bHandled=:bHandled WHERE strId=:strId", 
					use(bHandled), use(pCmd.strId), now;
	
	m_pDbTask.ReleaseDataSession();
	return TRUE;
	return TRUE;
}

inline void VEventServerSearchTask::run()
{
	while(m_Queue.BlockingPeek() == true)
	{
		VVidEventSearchCmd sCmd = m_Queue.Pop();
		VDC_DEBUG( "%s Pop a Event \n",__FUNCTION__);
		switch(sCmd.type)
		{
			case VVID_EVENT_SEARCH_CMD:
				ProcessSearchCmd(sCmd);
				break;
			case VVID_EVENT_HANDLE_CMD:
				ProcessHandleCmd(sCmd);
				break;
			default:
				break;
		}
	}
}

inline VEventServerDbTask::VEventServerDbTask(Factory &pFactory)
:m_Factory(pFactory),  m_nYear(0), m_nMonth(0), m_pSqlSession(NULL)
{
}

inline VEventServerDbTask::~VEventServerDbTask()
{
	if (m_pSqlSession)
	{
		delete m_pSqlSession;
	}
}

inline void VEventServerDbTask::PushEvent(VEventData &pData)
{
	m_Queue.Push(pData);

	return;
}


inline void VEventServerDbTask::UpdateDBSession(bool bIsFirst)
{
	std::time_t pTime = time(NULL);

	/* Get the Event Server Conf */
	VidEventDBConf cDBConf;
	m_Factory.GetEventDBConf(cDBConf);

	Poco::Timestamp pTimeStamp = Poco::Timestamp::fromEpochTime(pTime);

	/* Use next sec to check  */
	Poco::DateTime pTimeTime(pTimeStamp);

	int nYear = pTimeTime.year();
	int nMonth = pTimeTime.month();
	int nHour = pTimeTime.hour();

	if (bIsFirst != true)
	{

		/* Every day check if need new database */
		if (pTime%3600 != 0 || nHour != 0)
		{
			return;
		}

		if (nYear == m_nYear && nMonth == m_nMonth)
		{
			return;
		}
	}

	m_nYear = nYear;
	m_nMonth = nMonth;

	char strPlus[1024];
	sprintf(strPlus, "%d_%d", m_nYear, nMonth);
	
	if (cDBConf.ntype() == VID_DB_SQLITE)
	{
		if (m_pSqlSession != NULL)
		{
			delete m_pSqlSession;
			m_pSqlSession = NULL;
		}
		try
		{	
			Poco::Path path(cDBConf.strdbpath());
			Poco::File file(path);
			file.createDirectories();
			SQLite::Connector::registerConnector();
			VDC_DEBUG("Create the Event DB path %s\n", path.toString().c_str());
			
			Poco::Path filePath("eventdb.db");
			path.append(filePath);
			path.makeFile();
			VDC_DEBUG("Create the Event DB file %s\n", path.toString().c_str());

			m_pSqlSession = new Poco::Data::Session("SQLite", path.toString());
			*m_pSqlSession << "CREATE TABLE IF NOT EXISTS events "
				"(id INTEGER PRIMARY KEY, strId TEXT, strDevice TEXT, "
				"strDeviceName TEXT, strType TEXT, nTime INTEGER, strEvttime DATE, strDesc TEXT,"
				"bHandled INTEGER, strComments TEXT)", now;
		}
		catch (exception const &e)		
		{			
			VDC_DEBUG("%s Create SQLITE session failed \n", __FUNCTION__);
		}

	}else
	{
		VDC_DEBUG( "%s Do not support the DB TYPE \n",__FUNCTION__);
		ve_sleep(1000);
	}	
}

inline  Poco::Data::Session *VEventServerDbTask::GetDataSession()
{
	return m_pSqlSession;
}

inline void VEventServerDbTask::ReleaseDataSession()
{
	//TODO for the count
}

inline void VEventServerDbTask::run()
{
	/* Update Session first */
	UpdateDBSession(true);
	
	/* Check if need  create new DB file*/
	while(m_Queue.BlockingPeek() == true)
	{
		
		VEventData sData = m_Queue.Pop();
		/* Take the mutex for write */
		XGuard guard(m_cMutex);
		VDC_DEBUG( "%s Pop a Event \n",__FUNCTION__);
		/* Write the Data to DB */
		std::time_t pTime = time(NULL);
		if (pTime%3600 == 0)
		{
			//TODO update the session
			//UpdateDBSession(false);
		}
		if (m_pSqlSession == NULL)
		{
			VDC_DEBUG("%s Database session is NULL \n", __FUNCTION__);
			continue;
		}
		try
		{
			s64 handled = 0;
			astring strComment = "Add your comments ...";
			*m_pSqlSession << "INSERT INTO events VALUES(NULL, :strId, :strDevice, :strDeviceName, "
				":strType, :nTime, :strEvttime, :strDesc, :bHandled, :strComments)", 
			use(sData.strId), use(sData.strDevice), use(sData.strDeviceName), use(sData.strType), 
			use(sData.nTime), use(sData.strEvttime), use(sData.strDesc), 
			use(handled), use(strComment), now;
		}
		catch (exception const &e)
		{			
			VDC_DEBUG("%s insert event error \n", __FUNCTION__);
		}
	}
}
		


inline VEventServer::VEventServer(Factory &pFactory)
:m_Factory(pFactory), m_DbTask(pFactory), m_CallbackTask(pFactory), 
m_SearchTask(pFactory, m_DbTask)
{
}

inline VEventServer::~VEventServer()
{

}

inline void VEventServer::PushEvent(VEventData &pData)
{
	m_DbTask.PushEvent(pData);
	m_CallbackTask.PushEvent(pData);

	return;
}
/* Register the call back for the event */
inline BOOL VEventServer::RegEventNotify(void * pData, FunctionEventNotify callback)
{
	return m_CallbackTask.RegEventNotify(pData, callback);
}
inline BOOL VEventServer::UnRegEventNotify(void * pData)
{
	return m_CallbackTask.UnRegEventNotify(pData);
}

inline BOOL VEventServer::RegSearchEventNotify(void * pData, FunctionEventNotify callback)
{	
	return m_SearchTask.RegEventNotify(pData, callback);
}
inline BOOL VEventServer::UnRegSearchEventNotify(void * pData)
{	
	return m_SearchTask.UnRegEventNotify(pData);
}

inline BOOL VEventServer::SearchEvent(astring strId, s64 nStart, s64 nEnd, void *pData)
{
	VVidEventSearchCmd cmd;
	cmd.strId = strId;
	cmd.type = VVID_EVENT_SEARCH_CMD;
	cmd.nStart = nStart;
	cmd.nEnd = nEnd;
	cmd.pData = pData;
	
	return m_SearchTask.PushCmd(cmd);
}

inline BOOL VEventServer::HandleEvent(astring strId)
{
	VVidEventSearchCmd cmd;
	cmd.strId = strId;
	cmd.type = VVID_EVENT_HANDLE_CMD;
	
	return m_SearchTask.PushCmd(cmd);
}

inline BOOL VEventServer::Init()
{
	m_DbTask.start();
	m_CallbackTask.start();
	m_SearchTask.start();

	return TRUE;
}


#endif // __VSC_EVENT_SERVER_IMPL_H_
