# Database

SQLite database is utilized to store user information, group information, messages and message recipients. The database schema is designed to support user and group messaging. In this directory, you can find the database created during server initialization.

## Database Schema Documentation

This document describes the database schema for the messaging system, supporting user and group messaging.

## Tables

### 1. Users Table

Stores user information.

| Column        | Type    | Constraints                         |
|---------------|---------|-------------------------------------|
| `user_id`    | INTEGER | PRIMARY KEY AUTOINCREMENT           |
| `username`    | TEXT    | NOT NULL UNIQUE                     |
| `password_hash` | TEXT  | NOT NULL                            |
| `email`      | TEXT    | NOT NULL UNIQUE                     |
| `created_at` | TEXT    | DEFAULT CURRENT_TIMESTAMP           |
| `last_login` | TEXT    |                                     |

### 2. Groups Table

Stores group information.

| Column        | Type    | Constraints                                      |
|---------------|---------|--------------------------------------------------|
| `group_id`   | INTEGER | PRIMARY KEY AUTOINCREMENT                        |
| `group_name` | TEXT    | NOT NULL UNIQUE                                  |
| `created_by` | INTEGER | NOT NULL, FOREIGN KEY REFERENCES users(user_id) |
| `created_at` | TEXT    | DEFAULT CURRENT_TIMESTAMP                        |

### 3. Group Membership Table

Manages relationships between users and groups.

| Column          | Type    | Constraints                                      |
|-----------------|---------|--------------------------------------------------|
| `membership_id` | INTEGER | PRIMARY KEY AUTOINCREMENT                        |
| `group_id`      | INTEGER | NOT NULL, FOREIGN KEY REFERENCES groups(group_id)|
| `user_id`       | INTEGER | NOT NULL, FOREIGN KEY REFERENCES users(user_id) |
| `joined_at`     | TEXT    | DEFAULT CURRENT_TIMESTAMP                        |

### 4. Messages Table

Stores messages sent by users.

| Column          | Type    | Constraints                                      |
|-----------------|---------|--------------------------------------------------|
| `message_id`    | INTEGER | PRIMARY KEY AUTOINCREMENT                        |
| `sender_id`     | INTEGER | NOT NULL, FOREIGN KEY REFERENCES users(user_id) |
| `content`       | TEXT    | NOT NULL                                        |
| `created_at`    | TEXT    | DEFAULT CURRENT_TIMESTAMP                        |
| `is_group_message` | BOOLEAN | DEFAULT FALSE                                  |

### 5. Message Recipients Table

Tracks recipients for each message.

| Column                 | Type    | Constraints                                      |
|------------------------|---------|--------------------------------------------------|
| `recipient_id`         | INTEGER | PRIMARY KEY AUTOINCREMENT                        |
| `message_id`           | INTEGER | NOT NULL, FOREIGN KEY REFERENCES messages(message_id) |
| `recipient_user_id`    | INTEGER | FOREIGN KEY REFERENCES users(user_id)            |
| `recipient_group_id`   | INTEGER | FOREIGN KEY REFERENCES groups(group_id)          |
| CHECK (recipient_user_id IS NOT NULL OR recipient_group_id IS NOT NULL) |
| CHECK (recipient_user_id IS NULL OR recipient_group_id IS NULL) |

## Relationships

- **Users** can belong to multiple **Groups** through the **Group Membership** table.
- **Messages** can be sent by a **User** to another **User** or to a **Group**.
- **Message Recipients** allow multiple users to receive the same message.

## Diagrams

![ERD](/assets/erd.png)
